/* interrupts/isr.c - ISR handlers
 * FIXED: Use STACK_* macros (direct access) not ISR_STACK_* (dereferenced)
 */

#include "../kernel/kernel.h"
#include "../kernel/fpu.h"
#include "isr_stack.h"

/* Forward declarations */
extern void page_fault_handler(uint32_t *stack_ptr);
extern void pic_send_eoi(uint8_t irq);

/* Exception messages */
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
    "Reserved", "Reserved"
};

/* Hex/decimal printing helpers */
static void print_hex_digit(uint8_t val)
{
    if (val < 10)
        terminal_putchar('0' + val);
    else
        terminal_putchar('A' + val - 10);
}

static void print_hex32(uint32_t val)
{
    terminal_writestring("0x");
    for (int i = 7; i >= 0; i--)
    {
        print_hex_digit((val >> (i * 4)) & 0xF);
    }
}

static void print_dec(uint32_t val)
{
    if (val >= 100)
        terminal_putchar('0' + (val / 100) % 10);
    if (val >= 10)
        terminal_putchar('0' + (val / 10) % 10);
    terminal_putchar('0' + val % 10);
}

/* CPU Exception Handler - FIXED to use STACK_* macros (direct access) */
void isr_handler(uint32_t *stack_ptr)
{
    /* Use direct STACK_* macros - assembly passes ESP value, not pointer to ESP */
    uint32_t int_no = STACK_INTNO(stack_ptr);
    uint32_t err_code = STACK_ERRCODE(stack_ptr);

    /* === FPU / SSE EXCEPTIONS === */

    /* #7: Device Not Available */
    if (int_no == 7)
    {
        isr_device_not_available(stack_ptr);  /* Pass stack_ptr directly */
        return;
    }

    /* #16: x87 FPU Floating-Point Exception */
    if (int_no == 16)
    {
        isr_x87_fpu_fault(stack_ptr);  /* Pass stack_ptr directly */
        return;
    }

    /* #19: SIMD Floating-Point Exception */
    if (int_no == 19)
    {
        isr_simd_fp_exception(stack_ptr);  /* Pass stack_ptr directly */
        return;
    }

    /* SPECIAL CASE: Page fault (#14) */
    if (int_no == 14)
    {
        /* Pass stack_ptr directly - NO dereferencing needed! */
        page_fault_handler(stack_ptr);
        return;
    }

    /* === All other exceptions are fatal === */

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
        terminal_writestring(exception_messages[int_no]);
    else
        terminal_writestring("Unknown Exception");

    terminal_writestring("\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("Error code: ");
    print_hex32(err_code);
    terminal_writestring("\n");

    terminal_writestring("EIP: ");
    print_hex32(STACK_EIP(stack_ptr));  /* Use STACK_* not ISR_STACK_* */
    terminal_writestring("  CS: ");
    print_hex32(STACK_CS(stack_ptr));   /* Use STACK_* not ISR_STACK_* */
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

/* IRQ handler C wrapper - Uses STACK_* macros (direct, no indirection) */
void irq_handler_c(uint32_t *stack_ptr)
{
    uint32_t int_no = STACK_INTNO(stack_ptr);

    /* Ensure valid IRQ range */
    if (int_no < 32 || int_no > 47)
    {
        pic_send_eoi(0);
        return;
    }

    uint8_t irq = int_no - 32;

    /* Execute handler if installed */
    if (irq_handlers[irq])
    {
        irq_handlers[irq]();
    }

    /* Send EOI */
    pic_send_eoi(irq);
}

/* IRQ handler entry (naked) */
__attribute__((naked)) void irq_handler(void)
{
    __asm__ volatile(
        "mov %esp, %eax\n"
        "push %eax\n"
        "call irq_handler_c\n"
        "add $4, %esp\n"
        "ret\n");
}