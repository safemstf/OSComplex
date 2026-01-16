#include <stdint.h>
#include "../kernel/kernel.h"
#include "../drivers/terminal.h"

void fpu_init(void)
{
    uint32_t cr0, cr4;

    /* --- CR0 --- */
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // EM = 0
    cr0 |= (1 << 1);  // MP = 1
    cr0 |= (1 << 5);  // NE = 1
    asm volatile("mov %0, %%cr0" ::"r"(cr0));
    asm volatile("clts");

    /* --- CR4 --- */
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // OSFXSR
    cr4 |= (1 << 10); // OSXMMEXCPT
    asm volatile("mov %0, %%cr4" ::"r"(cr4));

    /* --- x87 init --- */
    asm volatile("fninit");
    asm volatile("fnclex"); // 🔴 clear pending exceptions

    /* Mask ALL x87 exceptions */
    uint16_t cw = 0x037F; // all masked, 64-bit precision
    asm volatile("fldcw %0" ::"m"(cw));

    /* --- SSE init --- */
    uint32_t mxcsr = 0x1F80; // mask all SSE exceptions
    asm volatile("ldmxcsr %0" ::"m"(mxcsr));
}

/* Called from the isr wrapper for vector 7 (Device Not Available / #NM) */
void isr_device_not_available(uint32_t *stack_ptr)
{
    /* Allow FPU/SSE usage by clearing TS, then init FPU state.
       Later, implement lazy FPU switching with fxsave/fxrstor per task. */
    (void)stack_ptr; // explicitly mark unused

    asm volatile("clts");   // clear task-switched
    asm volatile("fninit"); // initialize x87 FPU state
    /* if using SSE and fxsave area:
       asm volatile ("fxrstor (%0)" :: "r"(pointer_to_saved_area));
    */

    /* Return to the faulting instruction so it can run with FPU now. */
}

/* Called from the isr wrapper for vector 16 (#MF) */
void isr_x87_fpu_fault(uint32_t *stack_ptr)
{
    (void)stack_ptr; // explicitly mark unused

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n[EXC] x87 FPU exception (#MF)\n");
    terminal_writestring("System halted.\n");
    while (1)
        __asm__ volatile("cli; hlt");
}

/* Called from the isr wrapper for vector 19 (#XF) */
void isr_simd_fp_exception(uint32_t *stack_ptr)
{
    (void)stack_ptr; // explicitly mark unused

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n[EXC] SIMD FP exception (#XF)\n");
    terminal_writestring("System halted.\n");
    while (1)
        __asm__ volatile("cli; hlt");
}
