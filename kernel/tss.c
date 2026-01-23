/* kernel/tss.c - Task State Segment Implementation
 * 
 * The TSS tells the CPU which stack to use when transitioning
 * from user mode (Ring 3) to kernel mode (Ring 0).
 */

#include "tss.h"
#include "gdt.h"
#include "kernel.h"

/* ====================================================================
 * GLOBAL TSS
 * ==================================================================== */

static struct tss_entry tss;

/* ====================================================================
 * TSS INITIALIZATION
 * ==================================================================== */

void tss_init(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[TSS] Initializing Task State Segment...\n");
    
    /* Zero out entire TSS structure */
    memset(&tss, 0, sizeof(struct tss_entry));
    
    /* Set up critical fields:
     * 
     * ss0: Kernel data segment (0x10)
     *      When CPU switches to Ring 0, it loads this into SS
     * 
     * esp0: Kernel stack pointer
     *      Will be set later by tss_set_kernel_stack() before
     *      entering user mode. For now, set to 0.
     * 
     * iomap_base: I/O permission bitmap offset
     *      Set to sizeof(tss) to indicate "no I/O bitmap"
     */
    
    tss.ss0 = KERNEL_DS;           /* Use kernel data segment */
    tss.esp0 = 0;                  /* Will be set before user mode */
    tss.iomap_base = sizeof(struct tss_entry);
    
    /* Install TSS into GDT at entry 5 (0x28) */
    uint32_t tss_base = (uint32_t)&tss;
    uint32_t tss_limit = sizeof(struct tss_entry);
    
    gdt_set_tss(tss_base, tss_limit);
    
    /* Load TSS into task register */
    tss_flush();
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[TSS] TSS initialized at 0x");
    terminal_write_hex(tss_base);
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

/* ====================================================================
 * SET KERNEL STACK
 * ==================================================================== */

/* Set the kernel stack that will be used when syscalls happen
 * 
 * CRITICAL: This MUST be called before entering user mode!
 * 
 * When a user mode program does INT 0x80 (syscall), the CPU:
 * 1. Looks up esp0 and ss0 in the TSS
 * 2. Switches to that stack
 * 3. Pushes user SS, ESP, EFLAGS, CS, EIP
 * 4. Jumps to the syscall handler
 * 
 * If esp0 is wrong or not set, you'll get a triple fault!
 */
void tss_set_kernel_stack(uint32_t stack)
{
    tss.esp0 = stack;
}