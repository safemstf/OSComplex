/* interrupts/isr.c - Interrupt Service Routine handlers (FIXED)
 *
 * FIX: Added hex output and proper stack offset handling
 */

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
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved"};

/* Helper: Print a hex digit */
static void print_hex_digit(uint8_t val)
{
    if (val < 10)
        terminal_putchar('0' + val);
    else
        terminal_putchar('A' + val - 10);
}

/* Helper: Print 32-bit value in hex */
static void print_hex32(uint32_t val)
{
    terminal_writestring("0x");
    for (int i = 7; i >= 0; i--)
    {
        print_hex_digit((val >> (i * 4)) & 0xF);
    }
}

/* Helper: Print decimal value */
static void print_dec(uint32_t val)
{
    if (val >= 100)
        terminal_putchar('0' + (val / 100) % 10);
    if (val >= 10)
        terminal_putchar('0' + (val / 10) % 10);
    terminal_putchar('0' + val % 10);
}

/* CPU Exception Handler */
void isr_handler(void)
{
    uint32_t *stack_ptr;
    __asm__ volatile("mov %%esp, %0" : "=r"(stack_ptr));

    /* Stack layout after isr_common_stub:
     * [0]  = return address
     * [1]  = GS
     * [2]  = FS
     * [3]  = ES
     * [4]  = DS
     * [5]  = EDI (pusha)
     * [6]  = ESI
     * [7]  = EBP
     * [8]  = original ESP
     * [9]  = EBX
     * [10] = EDX
     * [11] = ECX
     * [12] = EAX
     * [13] = interrupt number
     * [14] = error code
     * [15] = EIP (from CPU)
     * [16] = CS
     * [17] = EFLAGS
     */
    uint32_t int_no = stack_ptr[13];
    uint32_t err_code = stack_ptr[14];

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n\n");
    terminal_writestring("╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║              KERNEL PANIC - CPU Exception                ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("Exception #");
    print_dec(int_no);
    terminal_writestring(" (");
    print_hex32(int_no);
    terminal_writestring("): ");

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    if (int_no < 32)
    {
        terminal_writestring(exception_messages[int_no]);
    }
    else
    {
        terminal_writestring("Unknown Exception");
    }
    terminal_writestring("\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("Error code: ");
    print_hex32(err_code);
    terminal_writestring("\n");

    terminal_writestring("EIP: ");
    print_hex32(stack_ptr[15]);
    terminal_writestring("  CS: ");
    print_hex32(stack_ptr[16]);
    terminal_writestring("\n");

    terminal_writestring("\nSystem halted.\n");
    __asm__ volatile("cli; hlt");
    while (1)
        __asm__ volatile("hlt");
}

/* IRQ handler function pointers */
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

/* Forward declaration */
void irq_handler_c(uint32_t *stack_ptr)
{
    uint32_t int_no = stack_ptr[13];

    /* 1. Ensure it's a valid IRQ range */
    if (int_no < 32 || int_no > 47)
    {
        // ... (Error handling remains the same)
        pic_send_eoi(0);
        return;
    }

    uint8_t irq = int_no - 32;

    /* 2. Execute the specific driver handler (e.g., keyboard_handler) */
    if (irq_handlers[irq])
    {
        irq_handlers[irq]();
    }

    /* 3. Send End of Interrupt to the PIC */
    pic_send_eoi(irq);
}

/* IRQ handler entry – MUST be naked */
__attribute__((naked)) void irq_handler(void)
{
    __asm__ volatile(
        "mov %esp, %eax\n" /* eax = pointer to IRQ stack frame */
        "push %eax\n"
        "call irq_handler_c\n"
        "add $4, %esp\n"
        "ret\n");
}
