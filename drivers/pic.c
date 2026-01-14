/* pic.c - 8259 PIC (Programmable Interrupt Controller) driver
 * 
 * THE PIC'S JOB:
 * The PIC manages hardware interrupts (IRQs). When a device like the
 * keyboard wants attention, it signals the PIC, which then interrupts
 * the CPU. The PIC translates IRQ numbers into interrupt numbers.
 * 
 * WHY WE REMAP THE PIC:
 * By default, the PIC maps IRQ 0-7 to interrupts 8-15 and IRQ 8-15
 * to interrupts 0x70-0x77. This conflicts with CPU exceptions (0-31)!
 * 
 * For example:
 * - IRQ 0 (timer) would trigger interrupt 8 (double fault exception)
 * - This is BAD and causes confusion
 * 
 * So we remap the PIC to use interrupts 32-47, which don't conflict.
 * 
 * PIC ARCHITECTURE:
 * We have TWO PICs (master and slave) in a cascaded configuration:
 * - Master PIC: Handles IRQ 0-7
 * - Slave PIC: Handles IRQ 8-15, connected to master's IRQ 2
 * 
 * Total: 15 usable IRQs (IRQ 2 is used for cascading)
 */

#include "../kernel/kernel.h"

/* PIC I/O port addresses
 * These are hardware-defined addresses we can't change */
#define PIC1_COMMAND 0x20  /* Master PIC command port */
#define PIC1_DATA    0x21  /* Master PIC data port */
#define PIC2_COMMAND 0xA0  /* Slave PIC command port */
#define PIC2_DATA    0xA1  /* Slave PIC data port */

/* PIC commands */
#define PIC_EOI      0x20  /* End Of Interrupt */

/* Initialization Command Words (ICW) for PIC setup */
#define ICW1_ICW4      0x01  /* ICW4 needed */
#define ICW1_SINGLE    0x02  /* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04  /* Call address interval 4 (8) */
#define ICW1_LEVEL     0x08  /* Level triggered (edge) mode */
#define ICW1_INIT      0x10  /* Initialization required */

#define ICW4_8086      0x01  /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO      0x02  /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE 0x08  /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C /* Buffered mode/master */
#define ICW4_SFNM      0x10  /* Special fully nested (not) */

/* Small delay for old hardware that needs time to process commands
 * 
 * The PIC is old hardware and sometimes needs a moment between commands.
 * We use port 0x80 (POST diagnostic port) for this - writing to it
 * does nothing but takes a few CPU cycles.
 */
static void io_wait(void) {
    outb(0x80, 0);
}

/* Initialize and remap the PIC
 * 
 * This is one of the trickier parts of OS development. The sequence
 * of commands here is very specific and follows Intel documentation.
 * 
 * What we're doing:
 * 1. Send initialization command to both PICs
 * 2. Remap IRQs to interrupts 32-47
 * 3. Set up master-slave relationship
 * 4. Set PICs to 8086 mode
 * 5. Unmask all interrupts (enable them)
 * 
 * After this, hardware interrupts will work!
 */
void pic_init(void) {
    /* Save current interrupt masks
     * We'll restore these after remapping */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    /* Start initialization sequence (ICW1)
     * ICW1_INIT | ICW1_ICW4 = 0x11 = "initialize and expect ICW4" */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    /* ICW2: Set interrupt vector offsets
     * 
     * Master PIC: IRQ 0-7 mapped to interrupts 32-39
     * Slave PIC:  IRQ 8-15 mapped to interrupts 40-47
     * 
     * This is the actual REMAPPING step that prevents conflicts
     * with CPU exceptions (0-31).
     */
    outb(PIC1_DATA, 32);    /* Master PIC offset: 32 */
    io_wait();
    outb(PIC2_DATA, 40);    /* Slave PIC offset: 40 */
    io_wait();
    
    /* ICW3: Set up cascading
     * 
     * Tell master PIC: "Slave PIC is connected to your IRQ 2"
     * Tell slave PIC:  "You are connected to master's IRQ 2"
     * 
     * The value 4 = binary 0100 = bit 2 set (IRQ 2)
     * The value 2 = binary 0010 = "I'm cascade identity 2"
     */
    outb(PIC1_DATA, 4);     /* Master: slave on IRQ 2 */
    io_wait();
    outb(PIC2_DATA, 2);     /* Slave: cascade identity */
    io_wait();
    
    /* ICW4: Set mode
     * 
     * ICW4_8086 = Use 8086/8088 mode (as opposed to 8080/8085)
     * This is required for x86 processors.
     */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    
    /* Restore saved masks
     * This re-enables whatever interrupts were enabled before */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

/* Send End Of Interrupt (EOI) signal
 * 
 * CRITICAL: After handling an interrupt, you MUST send EOI to the PIC.
 * If you don't, the PIC won't send any more interrupts of that type!
 * 
 * How it works:
 * - IRQs 0-7 (master PIC): Send EOI to master only
 * - IRQs 8-15 (slave PIC): Send EOI to BOTH slave and master
 *   (because slave is connected through master)
 * 
 * Parameters:
 * - irq: The IRQ number (0-15), NOT the interrupt number
 */
void pic_send_eoi(uint8_t irq) {
    /* If IRQ came from slave PIC (IRQ 8-15), send EOI to slave */
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    
    /* Always send EOI to master PIC
     * (Even for slave IRQs, because they go through the master) */
    outb(PIC1_COMMAND, PIC_EOI);
}