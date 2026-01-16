/* interrupts/pagefault.c - Page fault handler
 * 
 * Handles page faults with recovery for demand paging.
 */

#include "../kernel/kernel.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"

/* Page fault error code bits */
#define PF_PRESENT     0x01  /* 0 = not present, 1 = protection violation */
#define PF_WRITE       0x02  /* 0 = read, 1 = write */
#define PF_USER        0x04  /* 0 = kernel, 1 = user */
#define PF_RESERVED    0x08  /* 1 = reserved bit violation */
#define PF_INSTRUCTION 0x10  /* 1 = instruction fetch */

/* Helper to print hex without using printf */
static void print_hex(uint32_t val) {
    terminal_writestring("0x");
    char buf[16];
    utoa(val, buf, 16);
    terminal_writestring(buf);
}

/* Page fault handler */
void page_fault_handler(uint32_t *stack_ptr) {
    /* Get faulting address from CR2 */
    uint32_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
    
    uint32_t err_code = stack_ptr[14];  /* Error code from stack */
    uint32_t eip = stack_ptr[15];       /* EIP from stack */
    
    /* Display fault information */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n[PAGE FAULT] ");
    
    /* Describe the fault */
    if (err_code & PF_PRESENT) {
        terminal_writestring("Protection violation");
    } else {
        terminal_writestring("Page not present");
    }
    
    if (err_code & PF_WRITE) {
        terminal_writestring(" (write)");
    } else {
        terminal_writestring(" (read)");
    }
    
    if (err_code & PF_USER) {
        terminal_writestring(" [user mode]");
    } else {
        terminal_writestring(" [kernel mode]");
    }
    
    terminal_writestring("\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("Fault address: ");
    print_hex(fault_addr);
    terminal_writestring("\n");
    
    terminal_writestring("Error code: ");
    print_hex(err_code);
    terminal_writestring("\n");
    
    terminal_writestring("EIP: ");
    print_hex(eip);
    terminal_writestring("\n\n");
    
    /* ================================================================
     * ATTEMPT RECOVERY
     * ================================================================ */
    
    /* Case 1: Page not present in mapped region - try to allocate */
    if (!(err_code & PF_PRESENT)) {
        /* Check if fault is in kernel heap */
        if (fault_addr >= KERNEL_HEAP_START && fault_addr < KERNEL_HEAP_END) {
            void* phys_page = pmm_alloc_block();
            if (phys_page) {
                uint32_t page_addr = fault_addr & ~0xFFF;
                vmm_map_page(page_addr, (uint32_t)phys_page, 
                           VMM_PRESENT | VMM_WRITE);
                
                memset((void*)page_addr, 0, PAGE_SIZE);
                
                terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                terminal_writestring("[PAGE FAULT] Recovered - kernel heap page allocated\n");
                terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
                return;
            }
        }
        
        /* Check if fault is in user space (future) */
        if (fault_addr >= 0x10000000 && fault_addr < 0xC0000000) {
            void* phys_page = pmm_alloc_block();
            if (phys_page) {
                uint32_t page_addr = fault_addr & ~0xFFF;
                vmm_map_page(page_addr, (uint32_t)phys_page, 
                           VMM_PRESENT | VMM_WRITE | VMM_USER);
                
                memset((void*)page_addr, 0, PAGE_SIZE);
                
                terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                terminal_writestring("[PAGE FAULT] Recovered - user page allocated\n");
                terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
                return;
            }
        }
    }
    
    /* Case 2: Write to read-only page (future: copy-on-write) */
    if ((err_code & PF_PRESENT) && (err_code & PF_WRITE)) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("[PAGE FAULT] Write to read-only page (COW not implemented)\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    }
    
    /* ================================================================
     * UNRECOVERABLE - PANIC
     * ================================================================ */
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║                  KERNEL PANIC                            ║\n");
    terminal_writestring("║              Unhandled Page Fault                        ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\nRegister dump:\n");
    terminal_writestring("EAX="); print_hex(stack_ptr[12]);
    terminal_writestring(" EBX="); print_hex(stack_ptr[9]);
    terminal_writestring(" ECX="); print_hex(stack_ptr[11]);
    terminal_writestring(" EDX="); print_hex(stack_ptr[10]);
    terminal_writestring("\n");
    terminal_writestring("ESI="); print_hex(stack_ptr[6]);
    terminal_writestring(" EDI="); print_hex(stack_ptr[5]);
    terminal_writestring(" EBP="); print_hex(stack_ptr[7]);
    terminal_writestring(" ESP="); print_hex(stack_ptr[8]);
    terminal_writestring("\n");
    terminal_writestring("EIP="); print_hex(eip);
    terminal_writestring(" EFLAGS="); print_hex(stack_ptr[17]);
    terminal_writestring("\n");
    terminal_writestring("CS="); print_hex(stack_ptr[16]);
    terminal_writestring(" DS="); print_hex(stack_ptr[4]);
    terminal_writestring("\n\n");
    
    terminal_writestring("System halted.\n");
    
    /* Halt */
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}