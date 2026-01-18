/* drivers/ata.h - ATA/IDE Disk Driver Interface
 *
 * Implements PIO (Programmed I/O) mode for ATA drives.
 * Supports LBA28 addressing (up to 128GB drives).
 *
 * HARDWARE OVERVIEW:
 * - ATA uses I/O ports for communication
 * - Primary bus: ports 0x1F0-0x1F7 (IRQ 14)
 * - Secondary bus: ports 0x170-0x177 (IRQ 15)
 * - Master/Slave: Two drives per bus
 *
 * ADDRESSING MODES:
 * - CHS (Cylinder-Head-Sector): Legacy mode
 * - LBA28: Linear sector addressing (0 to 268,435,455 sectors)
 * - LBA48: Extended addressing (not implemented)
 */

#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <stdbool.h>

/* ====================================================================
 * ATA I/O PORTS
 * ==================================================================== */

/* Primary ATA bus (Master/Slave) */
#define ATA_PRIMARY_DATA       0x1F0   /* Data register (16-bit) */
#define ATA_PRIMARY_ERROR      0x1F1   /* Error register (read) */
#define ATA_PRIMARY_FEATURES   0x1F1   /* Features register (write) */
#define ATA_PRIMARY_SECCOUNT   0x1F2   /* Sector count */
#define ATA_PRIMARY_LBA_LOW    0x1F3   /* LBA low byte */
#define ATA_PRIMARY_LBA_MID    0x1F4   /* LBA mid byte */
#define ATA_PRIMARY_LBA_HIGH   0x1F5   /* LBA high byte */
#define ATA_PRIMARY_DRIVE      0x1F6   /* Drive/head select */
#define ATA_PRIMARY_STATUS     0x1F7   /* Status register (read) */
#define ATA_PRIMARY_COMMAND    0x1F7   /* Command register (write) */
#define ATA_PRIMARY_CONTROL    0x3F6   /* Device control register */

/* Secondary ATA bus (Master/Slave) */
#define ATA_SECONDARY_DATA     0x170
#define ATA_SECONDARY_ERROR    0x171
#define ATA_SECONDARY_FEATURES 0x171
#define ATA_SECONDARY_SECCOUNT 0x172
#define ATA_SECONDARY_LBA_LOW  0x173
#define ATA_SECONDARY_LBA_MID  0x174
#define ATA_SECONDARY_LBA_HIGH 0x175
#define ATA_SECONDARY_DRIVE    0x176
#define ATA_SECONDARY_STATUS   0x177
#define ATA_SECONDARY_COMMAND  0x177
#define ATA_SECONDARY_CONTROL  0x376

/* ====================================================================
 * ATA STATUS REGISTER BITS
 * ==================================================================== */

#define ATA_SR_BSY     0x80    /* Busy */
#define ATA_SR_DRDY    0x40    /* Drive ready */
#define ATA_SR_DF      0x20    /* Drive fault */
#define ATA_SR_DSC     0x10    /* Drive seek complete */
#define ATA_SR_DRQ     0x08    /* Data request ready */
#define ATA_SR_CORR    0x04    /* Corrected data */
#define ATA_SR_IDX     0x02    /* Index */
#define ATA_SR_ERR     0x01    /* Error */

/* ====================================================================
 * ATA ERROR REGISTER BITS
 * ==================================================================== */

#define ATA_ER_BBK     0x80    /* Bad block */
#define ATA_ER_UNC     0x40    /* Uncorrectable data */
#define ATA_ER_MC      0x20    /* Media changed */
#define ATA_ER_IDNF    0x10    /* ID not found */
#define ATA_ER_MCR     0x08    /* Media change request */
#define ATA_ER_ABRT    0x04    /* Command aborted */
#define ATA_ER_TK0NF   0x02    /* Track 0 not found */
#define ATA_ER_AMNF    0x01    /* No address mark */

/* ====================================================================
 * ATA COMMANDS
 * ==================================================================== */

#define ATA_CMD_READ_PIO       0x20    /* Read sectors with retry */
#define ATA_CMD_READ_PIO_EXT   0x24    /* Read sectors (LBA48) */
#define ATA_CMD_WRITE_PIO      0x30    /* Write sectors with retry */
#define ATA_CMD_WRITE_PIO_EXT  0x34    /* Write sectors (LBA48) */
#define ATA_CMD_CACHE_FLUSH    0xE7    /* Flush write cache */
#define ATA_CMD_IDENTIFY       0xEC    /* Identify drive */
#define ATA_CMD_PACKET         0xA0    /* ATAPI packet */

/* ====================================================================
 * DRIVE SELECTION
 * ==================================================================== */

#define ATA_MASTER             0xA0    /* Master drive select */
#define ATA_SLAVE              0xB0    /* Slave drive select */

/* ====================================================================
 * ATA DRIVE INFO
 * ==================================================================== */

typedef struct ata_drive_info {
    bool present;              /* Is drive detected? */
    bool is_atapi;             /* ATAPI device (CD-ROM)? */
    uint32_t sectors;          /* Total sectors (LBA28) */
    char model[41];            /* Model string */
    char serial[21];           /* Serial number */
    char firmware[9];          /* Firmware version */
} ata_drive_info_t;

/* ====================================================================
 * FUNCTION PROTOTYPES
 * ==================================================================== */

/* Initialize ATA subsystem - detect drives */
void ata_init(void);

/* Read a single sector (512 bytes) */
int ata_read_sector(uint8_t drive, uint32_t lba, uint8_t *buffer);

/* Write a single sector (512 bytes) */
int ata_write_sector(uint8_t drive, uint32_t lba, const uint8_t *buffer);

/* Read multiple sectors */
int ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint8_t *buffer);

/* Write multiple sectors */
int ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const uint8_t *buffer);

/* Flush write cache to disk */
int ata_flush_cache(uint8_t drive);

/* Get drive information */
ata_drive_info_t *ata_get_drive_info(uint8_t drive);

/* Drive identification (0=Primary Master, 1=Primary Slave, 2=Secondary Master, 3=Secondary Slave) */
#define ATA_PRIMARY_MASTER     0
#define ATA_PRIMARY_SLAVE      1
#define ATA_SECONDARY_MASTER   2
#define ATA_SECONDARY_SLAVE    3

/* Sector size constant */
#define ATA_SECTOR_SIZE        512

#endif /* ATA_H */