/* kernel/kernel.c - Main kernel initialization and loop
 *
 * LinuxComplex kernel entry point.
 * PMM integration added carefully and early.
 */

#include "kernel.h"
#include "../mm/pmm.h"
#include "../mm/paging.h"
#include "../mm/heap.h"
#include "../lib/string.h"

/* Linker-provided symbols (defined in linker.ld) */
extern uint8_t kernel_start;
extern uint8_t kernel_end;

/* Simple helpers */
static inline uint32_t align_down(uint32_t addr)
{
    return addr & ~(PAGE_SIZE - 1);
}

static inline uint32_t align_up(uint32_t addr)
{
    return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

/* Kernel entry point */
void kernel_main(void)
{
    /* =========================================================
     * Step 1: Terminal initialization
     * ========================================================= */
    terminal_initialize();

    terminal_writestring("╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║              LinuxComplex v0.1-alpha                     ║\n");
    terminal_writestring("║           An AI-Native Operating System                 ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_writestring("[KERNEL] Booting LinuxComplex...\n");

    /* =========================================================
     * Step 2: Physical Memory Manager (PMM)
     * =========================================================
     * For now we conservatively assume MEMORY_LIMIT (128MB).
     * PMM starts with all pages marked USED, then we selectively
     * free usable memory and re-reserve the kernel itself.
     */
    terminal_writestring("[PMM] Initializing physical memory manager...\n");

    pmm_init(MEMORY_LIMIT);

    /* Free all memory above 1MB (conservative early-kernel policy) */
    pmm_init_region((void *)0x100000, MEMORY_LIMIT - 0x100000);

    /* Reserve kernel image */
    uint32_t kstart = align_down((uint32_t)&kernel_start);
    uint32_t kend = align_up((uint32_t)&kernel_end);
    uint32_t ksize = kend - kstart;

    pmm_deinit_region((void *)kstart, ksize);

    terminal_writestring("[PMM] Kernel memory reserved\n");
    terminal_writestring("[PMM] Physical memory manager ready\n\n");

    /* Step 2.5: Enable paging */
    terminal_writestring("[PAGING] Setting up identity-mapped paging...\n");
    paging_init();
    terminal_writestring("[PAGING] Paging enabled\n\n");

    terminal_writestring("[HEAP] Initializing kernel heap...\n");
    heap_init();

    /* =========================================================
     * Step 3: Interrupt Descriptor Table
     * ========================================================= */
    terminal_writestring("[IDT] Initializing interrupt table...\n");
    idt_init();
    terminal_writestring("[IDT] Interrupt table ready\n");

    /* =========================================================
     * Step 4: PIC initialization
     * ========================================================= */
    terminal_writestring("[PIC] Remapping interrupt controller...\n");
    pic_init();
    terminal_writestring("[PIC] Interrupt controller ready\n");

    /* =========================================================
     * Step 5: Device drivers
     * ========================================================= */
    terminal_writestring("[DRIVERS] Initializing device drivers...\n");
    keyboard_init();
    terminal_writestring("[DRIVERS] All drivers initialized\n");

    /* =========================================================
     * Step 6: Enable interrupts
     * ========================================================= */
    terminal_writestring("[KERNEL] Enabling interrupts...\n");
    __asm__ volatile("sti");
    terminal_writestring("[KERNEL] Interrupts enabled\n\n");

    /* =========================================================
     * Step 7: AI subsystem
     * ========================================================= */
    terminal_writestring("[AI] Initializing AI learning system...\n");
    ai_init();
    terminal_writestring("[AI] AI system ready\n\n");

    /* =========================================================
     * Step 8: Shell
     * ========================================================= */
    terminal_writestring("[SHELL] Starting interactive shell...\n");
    shell_init();

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║            System initialization complete!               ║\n");
    terminal_writestring("║                 Type 'help' to begin                     ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    shell_run();

    /* =========================================================
     * Safety halt (should never reach here)
     * ========================================================= */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("[KERNEL] Fatal: shell returned unexpectedly\n");

    while (1)
    {
        __asm__ volatile("cli; hlt");
    }
}
