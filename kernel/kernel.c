/* kernel/kernel.c - Main kernel initialization
 * 
 * FIXED: Proper initialization order - VMM before heap!
 */

#include "kernel.h"
#include "fpu.h"

/* Linker symbols */
extern uint8_t kernel_start;
extern uint8_t kernel_end;

/* Helper functions */
static inline uint32_t align_down(uint32_t addr) {
    return addr & ~(PAGE_SIZE - 1);
}

static inline uint32_t align_up(uint32_t addr) {
    return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

/* Kernel entry point */
void kernel_main(void) {
    /* =========================================================
     * Step 1: Terminal (needed for debug output)
     * ========================================================= */
    terminal_initialize();

    terminal_writestring("╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║              LinuxComplex v0.1-alpha                     ║\n");
    terminal_writestring("║           An AI-Native Operating System                 ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");
    terminal_writestring("[KERNEL] Booting LinuxComplex...\n");

    fpu_init();

    /* =========================================================
     * Step 2: Physical Memory Manager
     * ========================================================= */
    terminal_writestring("[PMM] Initializing physical memory manager...\n");
    
    /* Initialize PMM with conservative memory limit */
    pmm_init(MEMORY_LIMIT);
    
    /* Free usable memory above 1MB */
    pmm_init_region((void*)0x100000, MEMORY_LIMIT - 0x100000);
    
    /* Reserve kernel image */
    uint32_t kstart = align_down((uint32_t)&kernel_start);
    uint32_t kend = align_up((uint32_t)&kernel_end);
    uint32_t ksize = kend - kstart;
    
    pmm_deinit_region((void*)kstart, ksize);
    
    terminal_writestring("[PMM] Kernel memory reserved: 0x");
    char buf[16];
    utoa(kstart, buf, 16);
    terminal_writestring(buf);
    terminal_writestring(" - 0x");
    utoa(kend, buf, 16);
    terminal_writestring(buf);
    terminal_writestring("\n");
    terminal_writestring("[PMM] Physical memory manager ready\n\n");

    /* =========================================================
     * Step 3: Paging (basic identity mapping)
     * ========================================================= */
    terminal_writestring("[PAGING] Setting up initial paging...\n");
    paging_init();
    terminal_writestring("[PAGING] Paging enabled\n\n");

    /* =========================================================
     * Step 4: Virtual Memory Manager
     * CRITICAL: Must come BEFORE heap!
     * ========================================================= */
    terminal_writestring("[VMM] Initializing virtual memory manager...\n");
    vmm_init();
    terminal_writestring("[VMM] Virtual memory manager ready\n\n");

    /* =========================================================
     * Step 5: Kernel Heap
     * NOW we can use kmalloc!
     * ========================================================= */
    terminal_writestring("[HEAP] Initializing kernel heap...\n");
    heap_init();
    terminal_writestring("[HEAP] Kernel heap ready\n\n");

    /* =========================================================
     * Step 6: Interrupt Descriptor Table
     * ========================================================= */
    terminal_writestring("[IDT] Initializing interrupt table...\n");
    idt_init();
    terminal_writestring("[IDT] Interrupt table ready\n");

    /* =========================================================
     * Step 7: PIC initialization
     * ========================================================= */
    terminal_writestring("[PIC] Remapping interrupt controller...\n");
    pic_init();
    terminal_writestring("[PIC] Interrupt controller ready\n");

    /* =========================================================
     * Step 8: Device drivers
     * ========================================================= */
    terminal_writestring("[DRIVERS] Initializing device drivers...\n");
    keyboard_init();
    terminal_writestring("[DRIVERS] All drivers initialized\n");

    /* =========================================================
     * Step 9: Enable interrupts
     * ========================================================= */
    terminal_writestring("[KERNEL] Enabling interrupts...\n");
    __asm__ volatile("sti");
    terminal_writestring("[KERNEL] Interrupts enabled - system ready!\n\n");

    /* =========================================================
     * Step 10: AI subsystem
     * ========================================================= */
    terminal_writestring("[AI] Initializing AI learning system...\n");
    ai_init();
    terminal_writestring("[AI] AI system ready\n\n");

    /* =========================================================
     * Step 11: Shell
     * ========================================================= */
    terminal_writestring("[SHELL] Starting interactive shell...\n");
    shell_init();

    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║            System initialization complete!               ║\n");
    terminal_writestring("║                 Type 'help' to begin                     ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    /* =========================================================
     * Step 12: Run shell (never returns)
     * ========================================================= */
    shell_run();

    /* Should never reach here */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n\n[KERNEL] FATAL: Shell returned unexpectedly!\n");

    while (1) {
        __asm__ volatile("cli; hlt");
    }
}