/* kernel/tss.h - Task State Segment
 * 
 * The TSS is required for privilege level transitions (Ring 3 -> Ring 0).
 * When a user mode program makes a syscall, the CPU needs to know which
 * kernel stack to use - that's what the TSS provides.
 */

#ifndef TSS_H
#define TSS_H

#include <stdint.h>

/* ====================================================================
 * TSS STRUCTURE
 * ==================================================================== */

/* Task State Segment - complete x86 structure
 * 
 * Most fields are unused in modern OSes (we don't use hardware
 * task switching), but esp0 and ss0 are critical - they tell
 * the CPU which stack to use when switching from Ring 3 to Ring 0.
 */
struct tss_entry
{
    uint32_t prev_tss;   /* Previous TSS (hardware task switching - unused) */
    uint32_t esp0;       /* CRITICAL: Kernel stack pointer for Ring 0 */
    uint32_t ss0;        /* CRITICAL: Kernel stack segment for Ring 0 */
    uint32_t esp1;       /* Stack pointer for Ring 1 (unused) */
    uint32_t ss1;        /* Stack segment for Ring 1 (unused) */
    uint32_t esp2;       /* Stack pointer for Ring 2 (unused) */
    uint32_t ss2;        /* Stack segment for Ring 2 (unused) */
    uint32_t cr3;        /* Page directory base (we manage this ourselves) */
    uint32_t eip;        /* Instruction pointer (unused) */
    uint32_t eflags;     /* CPU flags (unused) */
    uint32_t eax;        /* General purpose registers (all unused) */
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;         /* Segment selectors (all unused) */
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;        /* LDT selector (unused) */
    uint16_t trap;       /* Debug trap flag */
    uint16_t iomap_base; /* I/O map base address */
} __attribute__((packed));

/* ====================================================================
 * FUNCTION PROTOTYPES
 * ==================================================================== */

/* Initialize TSS */
void tss_init(void);

/* Set kernel stack for next privilege transition
 * Call this before entering user mode! */
void tss_set_kernel_stack(uint32_t stack);

/* Assembly function to load TSS */
extern void tss_flush(void);

#endif /* TSS_H */