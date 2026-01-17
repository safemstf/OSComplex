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
    asm volatile("fnclex"); // Clear pending exceptions

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

    /* Return to the faulting instruction so it can run with FPU now. */
}

/* Called from the isr wrapper for vector 16 (#MF)
 * 
 * #MF is DEFERRED - it means "a previous x87 instruction set an exception
 * bit that was never cleared". This is RECOVERABLE in early kernel boot.
 * 
 * Correct behavior:
 * 1. Clear the exception state (fnclex)
 * 2. Reinitialize FPU (fninit)
 * 3. Return (retry instruction)
 */
void isr_x87_fpu_fault(uint32_t *stack_ptr)
{
    (void)stack_ptr;

    /* Clear x87 exception flags */
    asm volatile("fnclex");
    
    /* Reinitialize FPU to known-good state */
    asm volatile("fninit");
    
    /* Restore control word (mask all exceptions) */
    uint16_t cw = 0x037F;
    asm volatile("fldcw %0" ::"m"(cw));

    /* Return - instruction will retry with clean FPU state */
}

/* Called from the isr wrapper for vector 19 (#XF)
 * 
 * #XF is raised for SSE/SIMD floating-point exceptions.
 * Like #MF, this is RECOVERABLE in early boot.
 * 
 * Correct behavior:
 * 1. Clear MXCSR exception bits
 * 2. Return (retry instruction)
 */
void isr_simd_fp_exception(uint32_t *stack_ptr)
{
    (void)stack_ptr;

    /* Read current MXCSR */
    uint32_t mxcsr;
    asm volatile("stmxcsr %0" : "=m"(mxcsr));
    
    /* Clear exception flags (bits 0-5) */
    mxcsr &= ~0x3F;
    
    /* Ensure all exceptions are masked (bits 7-12) */
    mxcsr |= 0x1F80;
    
    /* Restore cleaned MXCSR */
    asm volatile("ldmxcsr %0" ::"m"(mxcsr));

    /* Return - instruction will retry with clean SSE state */
}