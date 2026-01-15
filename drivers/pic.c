/* pic.c - 8259 PIC (Programmable Interrupt Controller) driver - FIXED
 * 
 * CRITICAL FIX: Explicitly unmask IRQ 1 (keyboard) instead of restoring
 * old masks that had it disabled.
 */

#include "../kernel/kernel.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI      0x20

#define ICW1_ICW4      0x01
#define ICW1_INIT      0x10
#define ICW4_8086      0x01

static void io_wait(void) {
    outb(0x80, 0);
}

void pic_init(void) {
    /* Start initialization sequence */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    /* Set interrupt vector offsets */
    outb(PIC1_DATA, 32);    /* Master: IRQ 0-7 → interrupts 32-39 */
    io_wait();
    outb(PIC2_DATA, 40);    /* Slave: IRQ 8-15 → interrupts 40-47 */
    io_wait();
    
    /* Set up cascading */
    outb(PIC1_DATA, 4);     /* Master: slave on IRQ 2 */
    io_wait();
    outb(PIC2_DATA, 2);     /* Slave: cascade identity */
    io_wait();
    
    /* Set mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    
    /* CRITICAL FIX: Explicitly enable IRQs instead of restoring old masks
     * 
     * The old code did: outb(PIC1_DATA, mask1) which restored BIOS masks
     * that had keyboard disabled. Now we explicitly enable what we need.
     * 
     * PIC mask bits: 1 = masked (disabled), 0 = unmasked (enabled)
     * 
     * Binary: 11111100
     *         ||||||||
     *         |||||||+-- Bit 0: IRQ 0 (timer) = 0 = ENABLED
     *         ||||||+--- Bit 1: IRQ 1 (keyboard) = 0 = ENABLED
     *         |||||+---- Bit 2: IRQ 2 (cascade) = 1 = MASKED (always)
     *         ||||+----- Bits 3-7: Other IRQs = 1 = MASKED
     * 
     * 0xFC = 11111100 binary
     */
    outb(PIC1_DATA, 0xFC);  /* Enable IRQ 0 (timer) and IRQ 1 (keyboard) */
    outb(PIC2_DATA, 0xFF);  /* Mask all slave IRQs for now */
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}