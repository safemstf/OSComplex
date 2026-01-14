/* kernel/kernel.c - Main kernel initialization and loop
 * 
 * This is the heart of LinuxComplex. After the bootloader loads us,
 * this is where execution begins.
 * 
 * CRITICAL INITIALIZATION ORDER:
 * 1. Terminal (so we can see what's happening)
 * 2. IDT (set up interrupt table)
 * 3. PIC (remap hardware interrupts)
 * 4. Device drivers (keyboard, etc.)
 * 5. Enable interrupts (STI instruction)
 * 6. Shell/AI systems
 * 7. Main loop
 */

#include "kernel.h"

/* Kernel entry point
 * 
 * Called by boot.s after initial setup.
 * This function never returns - it starts the shell and loops forever.
 */
void kernel_main(void) {
    /* Step 1: Initialize terminal first
     * We need this to display status messages and debug info */
    terminal_initialize();
    
    terminal_writestring("╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║              LinuxComplex v0.1-alpha                     ║\n");
    terminal_writestring("║           An AI-Native Operating System                 ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");
    
    terminal_writestring("[KERNEL] Booting LinuxComplex...\n");
    
    /* Step 2: Initialize IDT (Interrupt Descriptor Table)
     * This sets up the table that tells the CPU where to jump when
     * an interrupt occurs (keyboard press, timer tick, etc.) */
    terminal_writestring("[IDT] Initializing interrupt table...\n");
    idt_init();
    terminal_writestring("[IDT] Interrupt table ready\n");
    
    /* Step 3: Remap and initialize PIC (Programmable Interrupt Controller)
     * The PIC manages hardware interrupts. We remap it so IRQs don't
     * conflict with CPU exceptions. */
    terminal_writestring("[PIC] Remapping interrupt controller...\n");
    pic_init();
    terminal_writestring("[PIC] Interrupt controller ready\n");
    
    /* Step 4: Initialize device drivers
     * Each driver will register its interrupt handler with the IDT */
    terminal_writestring("[DRIVERS] Initializing device drivers...\n");
    keyboard_init();
    terminal_writestring("[DRIVERS] All drivers initialized\n");
    
    /* Step 5: CRITICAL - Enable interrupts!
     * Without this, no interrupts will fire even though everything is set up.
     * The CPU starts with interrupts disabled for safety. */
    terminal_writestring("[KERNEL] Enabling interrupts...\n");
    __asm__ volatile("sti");  /* Set Interrupt Flag - ENABLE INTERRUPTS */
    terminal_writestring("[KERNEL] Interrupts enabled - system ready!\n\n");
    
    /* Step 6: Initialize AI system */
    terminal_writestring("[AI] Initializing AI learning system...\n");
    ai_init();
    terminal_writestring("[AI] AI system ready\n\n");
    
    /* Step 7: Initialize shell */
    terminal_writestring("[SHELL] Starting interactive shell...\n");
    shell_init();
    
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║            System initialization complete!               ║\n");
    terminal_writestring("║                 Type 'help' to begin                     ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    /* Step 8: Run the shell
     * This never returns - the shell runs in an infinite loop
     * processing user commands */
    shell_run();
    
    /* We should never get here, but just in case... */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n\n[KERNEL] Unexpected return from shell! System halted.\n");
    
    /* Halt the CPU */
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}