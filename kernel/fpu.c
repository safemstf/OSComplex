#include "fpu.h"
#include <stdint.h>

void fpu_init(void)
{
    uint32_t cr0;

    /* Clear EM (emulation) and TS (task switched) */
    asm volatile (
        "mov %%cr0, %0"
        : "=r"(cr0)
    );

    cr0 &= ~(1 << 2);  // Clear EM
    cr0 &= ~(1 << 3);  // Clear TS

    asm volatile (
        "mov %0, %%cr0"
        :
        : "r"(cr0)
    );

    /* Initialize x87 FPU */
    asm volatile ("fninit");
}
