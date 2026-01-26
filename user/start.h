/* user/start.h - User Program Entry Point
 *
 * CRITICAL: This sets up segment registers before main() runs.
 * After IRET to user mode, DS/ES/FS/GS are still kernel segments!
 * We must set them to user data segment (0x23) before any memory access.
 *
 * Include this ONCE in your main program file, BEFORE any other code.
 */

#ifndef START_H
#define START_H

#include "ulib.h"

/* Forward declaration - user must implement this */
void main(void);

/* Entry point - MUST be naked to avoid function prologue */
__attribute__((section(".text.startup")))
__attribute__((noreturn))
__attribute__((naked))
void _start(void)
{
    __asm__ volatile(
        /* Set up user data segments (0x23) */
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        /* Call main */
        "call main\n"

        /* Exit with return value from main (in eax) */
        "push %%eax\n"
        "mov $0, %%eax\n"      /* SYS_EXIT */
        "pop %%ebx\n"          /* exit code */
        "int $0x80\n"

        /* Should never reach here */
        "1: hlt\n"
        "jmp 1b\n"
        ::: "ax"
    );
}

#endif /* START_H */
