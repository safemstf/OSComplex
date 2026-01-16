/* interrupts/pagefault.c - ABI-CORRECT VERSION
 *
 * Matches the CURRENT working ISR stack layout exactly.
 * NO structs. NO assumptions. NO magic.
 */

#include "../kernel/kernel.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "../drivers/terminal.h"

/* Page fault error code bits */
#define PF_PRESENT     0x01
#define PF_WRITE       0x02
#define PF_USER        0x04
#define PF_RESERVED    0x08
#define PF_INSTRUCTION 0x10

/* Stack layout indices (from ESP in isr_handler) */
#define STACK_INT_NO   13
#define STACK_ERRCODE  14
#define STACK_EIP      15
#define STACK_CS       16
#define STACK_EFLAGS   17

void page_fault_handler(uint32_t *stack_ptr)
{
    uint32_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));

    uint32_t err_code = stack_ptr[STACK_ERRCODE];
    uint32_t eip      = stack_ptr[STACK_EIP];

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n[PAGE FAULT] ");

    /* Decode fault type */
    if (err_code & PF_PRESENT)
        terminal_writestring("Protection violation");
    else
        terminal_writestring("Page not present");

    if (err_code & PF_WRITE)
        terminal_writestring(" (write)");
    else
        terminal_writestring(" (read)");

    if (err_code & PF_USER)
        terminal_writestring(" [user]");
    else
        terminal_writestring(" [kernel]");

    terminal_writestring("\nFault address: 0x");
    terminal_write_hex(fault_addr);
    terminal_writestring("\n");

    /* ============================================================
     * Attempt recovery: non-present page only
     * ============================================================ */
    if (!(err_code & PF_PRESENT))
    {
        uint32_t page_addr = fault_addr & ~0xFFF;

        /* Kernel heap */
        if (fault_addr >= KERNEL_HEAP_START && fault_addr < KERNEL_HEAP_END)
        {
            void *phys = pmm_alloc_block();
            if (phys)
            {
                vmm_map_page(
                    page_addr,
                    (uint32_t)phys,
                    VMM_PRESENT | VMM_WRITE
                );

                memset((void *)page_addr, 0, PAGE_SIZE);

                terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                terminal_writestring("[PAGE FAULT] ✓ Kernel page allocated\n");
                terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
                return;
            }
        }

        /* User space */
        if (fault_addr >= 0x10000000 && fault_addr < 0xC0000000)
        {
            void *phys = pmm_alloc_block();
            if (phys)
            {
                vmm_map_page(
                    page_addr,
                    (uint32_t)phys,
                    VMM_PRESENT | VMM_WRITE | VMM_USER
                );

                memset((void *)page_addr, 0, PAGE_SIZE);

                terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                terminal_writestring("[PAGE FAULT] ✓ User page allocated\n");
                terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
                return;
            }
        }
    }

    /* ============================================================
     * UNRECOVERABLE FAULT
     * ============================================================ */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║             UNHANDLED PAGE FAULT - PANIC                 ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\nEIP: 0x");
    terminal_write_hex(eip);
    terminal_writestring("\nSystem halted.\n");

    while (1)
        __asm__ volatile("cli; hlt");
}
