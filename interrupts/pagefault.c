/* interrupts/pagefault.c - Page Fault Handler with EXTENSIVE DEBUG
 * FIXED: Uses STACK_* macros (direct access)
 */

#include "../kernel/kernel.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "../drivers/terminal.h"
#include "isr_stack.h"

/* Page fault error code bits */
#define PF_PRESENT 0x01
#define PF_WRITE 0x02
#define PF_USER 0x04
#define PF_RESERVED 0x08
#define PF_INSTRUCTION 0x10

void page_fault_handler(uint32_t *stack_ptr)
{
    /* Disable interrupts to prevent nested faults */
    __asm__ volatile("cli");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== PAGE FAULT HANDLER ENTERED ===\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    uint32_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));

    /* Use STACK_* macros - stack_ptr is already the direct stack pointer */
    uint32_t err_code = STACK_ERRCODE(stack_ptr);
    uint32_t eip = STACK_EIP(stack_ptr);

    terminal_writestring("Fault address: 0x");
    terminal_write_hex(fault_addr);
    terminal_writestring("\n");
    
    terminal_writestring("Error code: 0x");
    terminal_write_hex(err_code);
    terminal_writestring("\n");
    
    terminal_writestring("EIP: 0x");
    terminal_write_hex(eip);
    terminal_writestring("\n");

    /* Decode fault type */
    terminal_writestring("Type: ");
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
    terminal_writestring("\n");

    /* ============================================================
     * Attempt recovery: non-present page only
     * ============================================================ */
    if (!(err_code & PF_PRESENT))
    {
        uint32_t page_addr = fault_addr & ~0xFFF;
        
        terminal_writestring("\nAttempting recovery for address 0x");
        terminal_write_hex(page_addr);
        terminal_writestring("...\n");

        /* Kernel heap */
        if (fault_addr >= KERNEL_HEAP_START && fault_addr < KERNEL_HEAP_END)
        {
            terminal_writestring("Step 1: Allocating physical page...\n");
            void *phys = pmm_alloc_block();

            if (!phys)
            {
                terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                terminal_writestring("ERROR: pmm_alloc_block() returned NULL!\n");
                goto unrecoverable;
            }

            terminal_writestring("Step 2: Got physical page at 0x");
            terminal_write_hex((uint32_t)phys);
            terminal_writestring("\n");

            terminal_writestring("Step 3: Calling vmm_map_page()...\n");
            
            /* CRITICAL: Make sure vmm_map_page doesn't trigger another fault! */
            vmm_map_page(page_addr, (uint32_t)phys, VMM_PRESENT | VMM_WRITE);

            terminal_writestring("Step 4: vmm_map_page() returned successfully\n");

            terminal_writestring("Step 5: Calling memset() to zero page...\n");
            
            /* CRITICAL: This memset might trigger another fault if mapping didn't work! */
            memset((void *)page_addr, 0, PAGE_SIZE);

            terminal_writestring("Step 6: memset() completed successfully\n");

            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
            terminal_writestring("\n[SUCCESS] Page fault recovered! Returning to program...\n");
            terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            
            /* Re-enable interrupts before returning */
            __asm__ volatile("sti");
            return;
        }

        /* User space */
        if (fault_addr >= 0x10000000 && fault_addr < 0xC0000000)
        {
            terminal_writestring("User space fault - not implemented yet\n");
            goto unrecoverable;
        }
    }

unrecoverable:
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

/* Test function for page fault recovery - callable from shell */
void test_page_fault_recovery(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║            Page Fault Recovery Test                     ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    /* Test 1: Access unmapped memory in heap range */
    terminal_writestring("[TEST 1] Accessing unmapped page at 0xC0500000...\n");
    terminal_writestring("         About to trigger page fault...\n");
    
    volatile uint32_t *test_ptr = (volatile uint32_t *)0xC0500000;

    terminal_writestring("         Writing 0xDEADBEEF...\n");
    *test_ptr = 0xDEADBEEF;

    terminal_writestring("         ✓ Write succeeded!\n");
    terminal_writestring("         Reading back value...\n");
    uint32_t value = *test_ptr;

    terminal_writestring("         Value = 0x");
    terminal_write_hex(value);
    terminal_writestring("\n");

    if (value == 0xDEADBEEF)
    {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("         ✓ SUCCESS - Page fault recovery works!\n");
    }
    else
    {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("         ✗ FAILED - Wrong value\n");
    }

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\n");
}