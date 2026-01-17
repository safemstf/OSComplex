/* interrupts/isr_stack.h - SINGLE SOURCE OF TRUTH
 * 
 * CRITICAL DIFFERENCE:
 * - ISR: Assembly does "push %eax; call isr_handler" so we get a pointer to ESP
 * - IRQ: Naked function does "push %eax; call irq_handler_c" so we get direct ESP
 * 
 * Therefore: ISR needs one level of indirection, IRQ doesn't.
 */

#ifndef ISR_STACK_H
#define ISR_STACK_H

#include <stdint.h>

/* ================================================================
 * ISR MACROS (for isr_handler)
 * Assembly passes pointer to ESP, so we dereference once
 * ================================================================ */
#define ISR_ACTUAL_STACK(sp) ((uint32_t*)(sp)[0])

#define ISR_STACK_GS(sp)        (ISR_ACTUAL_STACK(sp)[0])
#define ISR_STACK_FS(sp)        (ISR_ACTUAL_STACK(sp)[1])
#define ISR_STACK_ES(sp)        (ISR_ACTUAL_STACK(sp)[2])
#define ISR_STACK_DS(sp)        (ISR_ACTUAL_STACK(sp)[3])

#define ISR_STACK_EDI(sp)       (ISR_ACTUAL_STACK(sp)[4])
#define ISR_STACK_ESI(sp)       (ISR_ACTUAL_STACK(sp)[5])
#define ISR_STACK_EBP(sp)       (ISR_ACTUAL_STACK(sp)[6])
#define ISR_STACK_ESP_DUMMY(sp) (ISR_ACTUAL_STACK(sp)[7])
#define ISR_STACK_EBX(sp)       (ISR_ACTUAL_STACK(sp)[8])
#define ISR_STACK_EDX(sp)       (ISR_ACTUAL_STACK(sp)[9])
#define ISR_STACK_ECX(sp)       (ISR_ACTUAL_STACK(sp)[10])
#define ISR_STACK_EAX(sp)       (ISR_ACTUAL_STACK(sp)[11])

#define ISR_STACK_ERRCODE(sp)   (ISR_ACTUAL_STACK(sp)[12])
#define ISR_STACK_INTNO(sp)     (ISR_ACTUAL_STACK(sp)[13])
#define ISR_STACK_EIP(sp)       (ISR_ACTUAL_STACK(sp)[14])
#define ISR_STACK_CS(sp)        (ISR_ACTUAL_STACK(sp)[15])
#define ISR_STACK_EFLAGS(sp)    (ISR_ACTUAL_STACK(sp)[16])

/* ================================================================
 * IRQ/COMMON MACROS (for irq_handler_c and page_fault_handler)
 * Already dereferenced by naked function, direct access
 * ================================================================ */
#define STACK_GS(sp)        ((sp)[0])
#define STACK_FS(sp)        ((sp)[1])
#define STACK_ES(sp)        ((sp)[2])
#define STACK_DS(sp)        ((sp)[3])

#define STACK_EDI(sp)       ((sp)[4])
#define STACK_ESI(sp)       ((sp)[5])
#define STACK_EBP(sp)       ((sp)[6])
#define STACK_ESP_DUMMY(sp) ((sp)[7])
#define STACK_EBX(sp)       ((sp)[8])
#define STACK_EDX(sp)       ((sp)[9])
#define STACK_ECX(sp)       ((sp)[10])
#define STACK_EAX(sp)       ((sp)[11])

#define STACK_ERRCODE(sp)   ((sp)[12])
#define STACK_INTNO(sp)     ((sp)[13])
#define STACK_EIP(sp)       ((sp)[14])
#define STACK_CS(sp)        ((sp)[15])
#define STACK_EFLAGS(sp)    ((sp)[16])

#endif /* ISR_STACK_H */