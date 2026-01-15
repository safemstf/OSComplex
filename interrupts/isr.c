/* interrupts/isr.c - Interrupt Service Routine handlers */

#include "../kernel/kernel.h"

static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"};

void isr_handler(void)
{
    uint32_t *stack_ptr;
    __asm__ volatile("mov %%esp, %0" : "=r"(stack_ptr));

    uint32_t int_no = stack_ptr[13];

    if (int_no >= 32)
    {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("\n[ISR] Warning: Non-exception interrupt received\n");
        return;
    }

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n\n");
    terminal_writestring("╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║              KERNEL PANIC - CPU Exception                ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("Exception #");

    char num_str[3];
    num_str[0] = '0' + (int_no / 10);
    num_str[1] = '0' + (int_no % 10);
    num_str[2] = '\0';
    terminal_writestring(num_str);
    terminal_writestring(": ");

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring(exception_messages[int_no]);

    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("System halted.\n\n");

    __asm__ volatile("cli; hlt");

    while (1)
    {
        __asm__ volatile("hlt");
    }
}

static interrupt_handler_t irq_handlers[16];

void irq_install_handler(uint8_t irq, interrupt_handler_t handler)
{
    if (irq < 16)
    {
        irq_handlers[irq] = handler;
    }
}

void irq_uninstall_handler(uint8_t irq)
{
    if (irq < 16)
    {
        irq_handlers[irq] = 0;
    }
}

void irq_handler(void)
{
    uint32_t *stack_ptr;
    __asm__ volatile("mov %%esp, %0" : "=r"(stack_ptr));

    /* Read interrupt number from stack */
    uint32_t int_no = stack_ptr[13];

    /* CRITICAL FIX: Send EOI IMMEDIATELY to prevent recursive interrupts!
     * If we do ANY terminal output before sending EOI, the terminal code
     * can be slow enough that the same interrupt fires again, causing
     * infinite recursion and stack corruption.
     */
    uint8_t irq;
    if (int_no >= 32 && int_no <= 47)
    {
        irq = int_no - 32;
        pic_send_eoi(irq); /* Send EOI NOW, before any other processing */
    }
    else
    {
        /* Invalid interrupt - send EOI to master PIC as fallback */
        pic_send_eoi(0);
        return; /* Don't process further */
    }

    /* Now it's safe to do other work - EOI has been sent, so no recursion */

    /* Call registered handler if one exists */
    if (irq_handlers[irq])
    {
        irq_handlers[irq]();
    }

    /* Note: EOI already sent at the top, so we just return */
}