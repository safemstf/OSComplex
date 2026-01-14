/* idt.c - Interrupt Descriptor Table management
 * 
 * The IDT tells the CPU where to jump when an interrupt occurs.
 * It's an array of 256 entries, each describing one interrupt handler.
 * 
 * Why 256? Intel x86 supports up to 256 different interrupts:
 * - 0-31:   CPU exceptions (divide by zero, page fault, etc.) - Reserved by Intel
 * - 32-47:  Hardware interrupts (IRQs) - We remap PIC to use these
 * - 48-255: Available for software interrupts (system calls, etc.)
 * 
 * Critical OS concept: The IDT is how hardware communicates with software.
 * Without it, keyboard presses, timer ticks, and disk operations would be impossible.
 */

#include "../kernel/kernel.h"

/* The actual IDT - 256 entries, each 8 bytes = 2KB total */
static struct idt_entry idt[IDT_ENTRIES];

/* IDT pointer - tells CPU where our IDT is located in memory */
static struct idt_ptr idtp;

/* External assembly stub declarations (we'll write these next)
 * These are the actual interrupt handler entry points in assembly.
 * They handle CPU state saving/restoring before calling C functions.
 */
extern void isr0(void);   /* Divide by zero */
extern void isr1(void);   /* Debug */
extern void isr2(void);   /* Non-maskable interrupt */
extern void isr3(void);   /* Breakpoint */
extern void isr4(void);   /* Overflow */
extern void isr5(void);   /* Bound range exceeded */
extern void isr6(void);   /* Invalid opcode */
extern void isr7(void);   /* Device not available */
extern void isr8(void);   /* Double fault */
extern void isr9(void);   /* Coprocessor segment overrun */
extern void isr10(void);  /* Invalid TSS */
extern void isr11(void);  /* Segment not present */
extern void isr12(void);  /* Stack segment fault */
extern void isr13(void);  /* General protection fault */
extern void isr14(void);  /* Page fault */
extern void isr15(void);  /* Reserved */
extern void isr16(void);  /* x87 floating point exception */
extern void isr17(void);  /* Alignment check */
extern void isr18(void);  /* Machine check */
extern void isr19(void);  /* SIMD floating point exception */
extern void isr20(void);  /* Virtualization exception */
extern void isr21(void);  /* Control protection exception */
extern void isr22(void);  /* Reserved */
extern void isr23(void);  /* Reserved */
extern void isr24(void);  /* Reserved */
extern void isr25(void);  /* Reserved */
extern void isr26(void);  /* Reserved */
extern void isr27(void);  /* Reserved */
extern void isr28(void);  /* Reserved */
extern void isr29(void);  /* Reserved */
extern void isr30(void);  /* Reserved */
extern void isr31(void);  /* Reserved */

/* IRQ handlers (hardware interrupts - keyboard, timer, etc.) */
extern void irq0(void);   /* Timer */
extern void irq1(void);   /* Keyboard */
extern void irq2(void);   /* Cascade (never raised) */
extern void irq3(void);   /* COM2 */
extern void irq4(void);   /* COM1 */
extern void irq5(void);   /* LPT2 */
extern void irq6(void);   /* Floppy disk */
extern void irq7(void);   /* LPT1 */
extern void irq8(void);   /* CMOS real-time clock */
extern void irq9(void);   /* Free for peripherals */
extern void irq10(void);  /* Free for peripherals */
extern void irq11(void);  /* Free for peripherals */
extern void irq12(void);  /* PS/2 mouse */
extern void irq13(void);  /* FPU / coprocessor / inter-processor */
extern void irq14(void);  /* Primary ATA hard disk */
extern void irq15(void);  /* Secondary ATA hard disk */

/* Set up one IDT entry
 * 
 * Parameters:
 * - num: Interrupt number (0-255)
 * - handler: Address of the handler function
 * - selector: Code segment selector (0x08 = kernel code segment)
 * - flags: Type and privilege level
 * 
 * Flags format:
 * Bit 7:    Present (must be 1 for valid entry)
 * Bit 6-5:  DPL (privilege level: 0=kernel, 3=user)
 * Bit 4:    Storage segment (0 for interrupt gates)
 * Bit 3-0:  Gate type (0xE = 32-bit interrupt gate, 0xF = 32-bit trap gate)
 * 
 * Common flag values:
 * 0x8E = Present, ring 0, 32-bit interrupt gate (disables interrupts)
 * 0x8F = Present, ring 0, 32-bit trap gate (doesn't disable interrupts)
 */
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[num].base_low = handler & 0xFFFF;          /* Lower 16 bits of handler address */
    idt[num].base_high = (handler >> 16) & 0xFFFF; /* Upper 16 bits of handler address */
    idt[num].selector = selector;                   /* Code segment selector */
    idt[num].zero = 0;                             /* Always zero */
    idt[num].flags = flags;                        /* Type and attributes */
}

/* Initialize the IDT
 * 
 * This function:
 * 1. Sets up the IDT pointer structure
 * 2. Installs handlers for CPU exceptions (0-31)
 * 3. Installs handlers for hardware IRQs (32-47)
 * 4. Loads the IDT using the LIDT instruction
 * 
 * After this, interrupts can be enabled safely.
 */
void idt_init(void) {
    /* Set up IDT pointer structure */
    idtp.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idtp.base = (uint32_t)&idt;
    
    /* Clear the entire IDT (all entries invalid by default) */
    memset(&idt, 0, sizeof(struct idt_entry) * IDT_ENTRIES);
    
    /* Install CPU exception handlers (interrupts 0-31)
     * These are defined by Intel and handle CPU errors/faults.
     * 0x08 = kernel code segment selector
     * 0x8E = present, ring 0, 32-bit interrupt gate
     */
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);
    
    /* Install IRQ handlers (interrupts 32-47)
     * The PIC is remapped to use these interrupt numbers to avoid
     * conflicts with CPU exceptions.
     */
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);   /* Timer */
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);   /* Keyboard */
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);
    
    /* Load the IDT - tells CPU where to find interrupt handlers
     * After this instruction, the CPU knows about our interrupt table */
    __asm__ volatile("lidt %0" : : "m"(idtp));
}