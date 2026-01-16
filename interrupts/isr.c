/* interrupts/isr.c - ISR handlers (safe drop-in)
 * Minimal improvements only: macros, prototypes, no ABI changes.
 */

#include "../kernel/kernel.h"

/* Forward declarations (from other C files) to avoid implicit declarations */
extern void page_fault_handler(uint32_t *stack_ptr);  /* keeps your existing PF handler signature */
extern void pic_send_eoi(uint8_t irq);                /* from drivers/pic.c */

/* Keep your exception messages exactly as before */
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

/* --- small helpers for hex/decimal printing (unchanged semantics) --- */
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

/* --- Accessor macros that encapsulate the magic indices.
   Keep these so you can switch to a struct later by editing just these
   macros (no assembly changes needed). --- */
#define STACK_INTNO(sp)   ((sp)[13])
#define STACK_ERRCODE(sp) ((sp)[14])
#define STACK_EIP(sp)     ((sp)[15])
#define STACK_CS(sp)      ((sp)[16])

/* CPU Exception Handler (keeps original signature and behavior) */
void isr_handler(void)
{
    uint32_t *stack_ptr;
    __asm__ volatile("mov %%esp, %0" : "=r"(stack_ptr));

    uint32_t int_no = STACK_INTNO(stack_ptr);
    uint32_t err_code = STACK_ERRCODE(stack_ptr);

    /* SPECIAL CASE: Page fault (#14) - forward to your handler */
    if (int_no == 14)
    {
        page_fault_handler(stack_ptr);
        return; /* If it returns, recovery succeeded */
    }

    /* All other exceptions are fatal */
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
    print_hex32(STACK_EIP(stack_ptr));
    terminal_writestring("  CS: ");
    print_hex32(STACK_CS(stack_ptr));
    terminal_writestring("\n");

    terminal_writestring("\nSystem halted.\n");
    __asm__ volatile("cli; hlt");
    while (1)
        __asm__ volatile("hlt");
}

/* IRQ handler function pointers (unchanged) */
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

/* IRQ handler C wrapper (keeps original ABI exactly) */
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

/* IRQ handler entry (naked) - unchanged (keeps your working stub) */
__attribute__((naked)) void irq_handler(void)
{
    __asm__ volatile(
        "mov %esp, %eax\n"
        "push %eax\n"
        "call irq_handler_c\n"
        "add $4, %esp\n"
        "ret\n");
}
