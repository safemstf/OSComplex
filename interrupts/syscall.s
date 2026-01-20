/* kernel/syscall.s - System Call Entry Point
 * 
 * CRITICAL FIX: This MUST match struct registers layout EXACTLY:
 * 
 * struct registers {
 *     uint32_t ds;                                       ← Push FIRST
 *     uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  ← pusha order
 *     uint32_t int_no, err_code;                        ← Push BEFORE pusha
 *     uint32_t eip, cs, eflags, useresp, ss;            ← CPU already pushed
 * } __attribute__((packed));
 * 
 * CPU state when INT 0x80 fires:
 * [SS] [ESP] [EFLAGS] [CS] [EIP] ← CPU pushed (bottom of stack)
 * 
 * We need to build the rest to match struct registers
 */

.section .text
.global syscall_stub
.type syscall_stub, @function

syscall_stub:
    cli
    
    /* INT 0x80 doesn't push error code, so we fake it */
    push $0                     /* err_code (fake) */
    push $0x80                  /* int_no */
    
    /* Save general purpose registers */
    pusha                       /* Pushes: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI */
    
    /* Save segment register */
    push %ds                    /* Now DS is at the TOP (matches struct!) */
    
    /* Load kernel segments */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    /* Call C handler - ESP points to complete struct registers */
    push %esp
    call syscall_handler
    add $4, %esp
    
    /* Restore in REVERSE order */
    pop %ds                     /* Restore DS first */
    popa                        /* Restore general purpose registers */
    add $8, %esp                /* Remove int_no and err_code */
    
    /* Return to user mode */
    sti
    iret

.size syscall_stub, . - syscall_stub

.section .note.GNU-stack,"",@progbits
