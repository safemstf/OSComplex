/* interrupts/syscall.s - System Call Entry Point
 * 
 * INT 0x80 handler - saves registers and calls syscall_handler
 */

.section .text
.global syscall_stub

syscall_stub:
    cli
    
    /* Save all registers */
    pusha
    
    push %ds
    push %es
    push %fs
    push %gs
    
    /* Load kernel data segment */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    /* Call C handler - pass pointer to register struct */
    mov %esp, %eax
    push %eax
    call syscall_handler
    add $4, %esp
    
    /* Restore segments */
    pop %gs
    pop %fs
    pop %es
    pop %ds
    
    /* Restore registers */
    popa
    
    sti
    iret

.section .note.GNU-stack,"",@progbits
