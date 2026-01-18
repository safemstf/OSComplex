/* drivers/ata.c - ATA/IDE Disk Driver Implementation
 *
 * This is a PIO (Programmed I/O) mode driver for ATA hard disks.
 * It's simple but reliable - perfect for learning!
 *
 * HOW ATA WORKS (Simplified):
 * 1. Send sector address (LBA) to I/O ports
 * 2. Send READ or WRITE command
 * 3. Wait for drive to be ready (poll status)
 * 4. Transfer data 16 bits at a time (in/out words)
 *
 * WHY PIO?
 * - Simple to implement
 * - No DMA setup needed
 * - Works on all ATA drives
 * - Good enough for an OS kernel!
 *
 * LIMITATIONS:
 * - CPU does all data transfers (slow)
 * - Blocks during I/O
 * - No interrupt handling (polls status)
 */

#include "ata.h"
#include "../kernel/kernel.h"

/* ====================================================================
 * GLOBAL STATE
 * ==================================================================== */

static ata_drive_info_t drives[4];  /* Primary Master/Slave, Secondary Master/Slave */

/* ====================================================================
 * HELPER FUNCTIONS
 * ==================================================================== */

/* Read 16-bit word from ATA data port */
static inline uint16_t ata_inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* Write 16-bit word to ATA data port */
static inline void ata_outw(uint16_t port, uint16_t data) {
    __asm__ volatile("outw %0, %1" :: "a"(data), "Nd"(port));
}

/* 400ns delay - read status port 15 times (required by ATA spec) */
static void ata_io_wait(uint16_t port) {
    for (int i = 0; i < 15; i++) {
        inb(port);
    }
}

/* Wait for drive to clear BSY bit
 * Returns 0 on success, -1 on timeout */
static int ata_wait_bsy(uint16_t status_port) {
    uint32_t timeout = 100000;
    
    while (timeout--) {
        uint8_t status = inb(status_port);
        if (!(status & ATA_SR_BSY)) {
            return 0;  /* BSY cleared */
        }
    }
    
    return -1;  /* Timeout */
}

/* Wait for drive to set DRQ bit (data ready)
 * Returns 0 on success, -1 on error/timeout */
static int ata_wait_drq(uint16_t status_port) {
    uint32_t timeout = 100000;
    
    while (timeout--) {
        uint8_t status = inb(status_port);
        
        /* Check for errors */
        if (status & ATA_SR_ERR) {
            return -1;  /* Error */
        }
        if (status & ATA_SR_DF) {
            return -1;  /* Drive fault */
        }
        
        /* Check if data is ready */
        if (status & ATA_SR_DRQ) {
            return 0;  /* Data ready */
        }
    }
    
    return -1;  /* Timeout */
}

/* Get I/O port base for a drive */
static uint16_t ata_get_port_base(uint8_t drive) {
    if (drive < 2) {
        return 0x1F0;  /* Primary bus */
    } else {
        return 0x170;  /* Secondary bus */
    }
}

/* Get control port for a drive */
static uint16_t ata_get_control_port(uint8_t drive) {
    if (drive < 2) {
        return 0x3F6;  /* Primary control */
    } else {
        return 0x376;  /* Secondary control */
    }
}

/* ====================================================================
 * DRIVE IDENTIFICATION
 * ==================================================================== */

/* Execute IDENTIFY command and fill drive info
 * Returns 0 on success, -1 if no drive present */
static int ata_identify(uint8_t drive) {
    uint16_t port_base = ata_get_port_base(drive);
    uint16_t control_port = ata_get_control_port(drive);
    bool is_slave = (drive % 2) == 1;
    
    /* Select drive (master=0xA0, slave=0xB0) */
    outb(port_base + 6, is_slave ? ATA_SLAVE : ATA_MASTER);
    ata_io_wait(port_base + 7);  /* 400ns delay */
    
    /* Set sector count and LBA registers to 0 */
    outb(port_base + 2, 0);
    outb(port_base + 3, 0);
    outb(port_base + 4, 0);
    outb(port_base + 5, 0);
    
    /* Send IDENTIFY command */
    outb(port_base + 7, ATA_CMD_IDENTIFY);
    ata_io_wait(port_base + 7);
    
    /* Read status */
    uint8_t status = inb(port_base + 7);
    if (status == 0) {
        /* No drive */
        drives[drive].present = false;
        return -1;
    }
    
    /* Wait for BSY to clear */
    if (ata_wait_bsy(port_base + 7) < 0) {
        drives[drive].present = false;
        return -1;
    }
    
    /* Check if this is an ATAPI device */
    uint8_t lba_mid = inb(port_base + 4);
    uint8_t lba_high = inb(port_base + 5);
    
    if (lba_mid != 0 || lba_high != 0) {
        /* ATAPI device - we don't support these yet */
        drives[drive].present = true;
        drives[drive].is_atapi = true;
        return -1;
    }
    
    /* Wait for DRQ */
    if (ata_wait_drq(port_base + 7) < 0) {
        drives[drive].present = false;
        return -1;
    }
    
    /* Read IDENTIFY data (256 words = 512 bytes) */
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = ata_inw(port_base);
    }
    
    /* Parse IDENTIFY data */
    drives[drive].present = true;
    drives[drive].is_atapi = false;
    
    /* Extract model string (words 27-46, 40 chars) */
    for (int i = 0; i < 20; i++) {
        uint16_t word = identify_data[27 + i];
        drives[drive].model[i * 2] = (word >> 8) & 0xFF;
        drives[drive].model[i * 2 + 1] = word & 0xFF;
    }
    drives[drive].model[40] = '\0';
    
    /* Extract serial number (words 10-19, 20 chars) */
    for (int i = 0; i < 10; i++) {
        uint16_t word = identify_data[10 + i];
        drives[drive].serial[i * 2] = (word >> 8) & 0xFF;
        drives[drive].serial[i * 2 + 1] = word & 0xFF;
    }
    drives[drive].serial[20] = '\0';
    
    /* Extract firmware version (words 23-26, 8 chars) */
    for (int i = 0; i < 4; i++) {
        uint16_t word = identify_data[23 + i];
        drives[drive].firmware[i * 2] = (word >> 8) & 0xFF;
        drives[drive].firmware[i * 2 + 1] = word & 0xFF;
    }
    drives[drive].firmware[8] = '\0';
    
    /* Get total sectors (LBA28) - words 60-61 */
    drives[drive].sectors = ((uint32_t)identify_data[61] << 16) | identify_data[60];
    
    return 0;
}

/* ====================================================================
 * INITIALIZATION
 * ==================================================================== */

void ata_init(void) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[ATA] Initializing ATA disk driver...\n");
    
    /* Clear drive info */
    memset(drives, 0, sizeof(drives));
    
    /* Detect drives on primary bus */
    terminal_writestring("[ATA] Detecting Primary Master... ");
    if (ata_identify(ATA_PRIMARY_MASTER) == 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("FOUND\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("      Model: ");
        terminal_writestring(drives[ATA_PRIMARY_MASTER].model);
        terminal_writestring("\n");
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK));
        terminal_writestring("Not present\n");
    }
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[ATA] Detecting Primary Slave... ");
    if (ata_identify(ATA_PRIMARY_SLAVE) == 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("FOUND\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("      Model: ");
        terminal_writestring(drives[ATA_PRIMARY_SLAVE].model);
        terminal_writestring("\n");
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK));
        terminal_writestring("Not present\n");
    }
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[ATA] ATA initialization complete\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

/* ====================================================================
 * SECTOR READ/WRITE
 * ==================================================================== */

int ata_read_sector(uint8_t drive, uint32_t lba, uint8_t *buffer) {
    if (drive >= 4 || !drives[drive].present) {
        return -1;
    }
    
    uint16_t port_base = ata_get_port_base(drive);
    bool is_slave = (drive % 2) == 1;
    
    /* Wait for drive to be ready */
    if (ata_wait_bsy(port_base + 7) < 0) {
        return -1;
    }
    
    /* Select drive and set LBA mode */
    outb(port_base + 6, (is_slave ? 0xF0 : 0xE0) | ((lba >> 24) & 0x0F));
    
    /* Set sector count to 1 */
    outb(port_base + 2, 1);
    
    /* Set LBA */
    outb(port_base + 3, lba & 0xFF);
    outb(port_base + 4, (lba >> 8) & 0xFF);
    outb(port_base + 5, (lba >> 16) & 0xFF);
    
    /* Send READ command */
    outb(port_base + 7, ATA_CMD_READ_PIO);
    
    /* Wait for DRQ */
    if (ata_wait_drq(port_base + 7) < 0) {
        return -1;
    }
    
    /* Read data (256 words = 512 bytes) */
    uint16_t *buf16 = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        buf16[i] = ata_inw(port_base);
    }
    
    /* 400ns delay */
    ata_io_wait(port_base + 7);
    
    return 0;
}

int ata_write_sector(uint8_t drive, uint32_t lba, const uint8_t *buffer) {
    if (drive >= 4 || !drives[drive].present) {
        return -1;
    }
    
    uint16_t port_base = ata_get_port_base(drive);
    bool is_slave = (drive % 2) == 1;
    
    /* Wait for drive to be ready */
    if (ata_wait_bsy(port_base + 7) < 0) {
        return -1;
    }
    
    /* Select drive and set LBA mode */
    outb(port_base + 6, (is_slave ? 0xF0 : 0xE0) | ((lba >> 24) & 0x0F));
    
    /* Set sector count to 1 */
    outb(port_base + 2, 1);
    
    /* Set LBA */
    outb(port_base + 3, lba & 0xFF);
    outb(port_base + 4, (lba >> 8) & 0xFF);
    outb(port_base + 5, (lba >> 16) & 0xFF);
    
    /* Send WRITE command */
    outb(port_base + 7, ATA_CMD_WRITE_PIO);
    
    /* Wait for DRQ */
    if (ata_wait_drq(port_base + 7) < 0) {
        return -1;
    }
    
    /* Write data (256 words = 512 bytes) */
    const uint16_t *buf16 = (const uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        ata_outw(port_base, buf16[i]);
    }
    
    /* Flush cache */
    outb(port_base + 7, ATA_CMD_CACHE_FLUSH);
    
    /* Wait for completion */
    ata_wait_bsy(port_base + 7);
    
    return 0;
}

int ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint8_t *buffer) {
    for (uint8_t i = 0; i < count; i++) {
        if (ata_read_sector(drive, lba + i, buffer + (i * 512)) < 0) {
            return i;  /* Return number of sectors successfully read */
        }
    }
    return count;
}

int ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const uint8_t *buffer) {
    for (uint8_t i = 0; i < count; i++) {
        if (ata_write_sector(drive, lba + i, buffer + (i * 512)) < 0) {
            return i;  /* Return number of sectors successfully written */
        }
    }
    return count;
}

int ata_flush_cache(uint8_t drive) {
    if (drive >= 4 || !drives[drive].present) {
        return -1;
    }
    
    uint16_t port_base = ata_get_port_base(drive);
    
    outb(port_base + 7, ATA_CMD_CACHE_FLUSH);
    return ata_wait_bsy(port_base + 7);
}

ata_drive_info_t *ata_get_drive_info(uint8_t drive) {
    if (drive >= 4) {
        return NULL;
    }
    return &drives[drive];
}