/* kernel/syscall_stub.s - System Call Entry Point
 * 
 * This MUST match struct registers layout exactly:
 * 
 * struct registers {
 *     uint32_t ds;
 *     uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
 *     uint32_t int_no, err_code;
 *     uint32_t eip, cs, eflags, useresp, ss;
 * } __attribute__((packed));
 */

.section .text
.global syscall_stub
.type syscall_stub, @function

syscall_stub:
    cli
    
    /* CRITICAL: INT 0x80 doesn't push error code!
     * Push fake error code to match struct registers */
    push $0                     /* err_code (fake) */
    push $0x80                  /* int_no */
    
    /* Save general purpose registers (matches pusha order) */
    pusha                       /* Pushes: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI */
    
    /* Save data segment */
    push %ds
    
    /* Load kernel data segment */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    /* Call C handler - pass pointer to struct registers */
    push %esp
    call syscall_handler
    add $4, %esp
    
    /* Restore data segment */
    pop %ds
    
    /* Restore general purpose registers */
    popa
    
    /* Remove int_no and err_code */
    add $8, %esp
    
    /* Return to user mode */
    sti
    iret

.size syscall_stub, . - syscall_stub

.section .note.GNU-stack,"",@progbits
