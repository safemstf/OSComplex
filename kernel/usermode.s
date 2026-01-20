/* kernel/usermode.s - Enter User Mode (Ring 3)
 * 
 * This function sets up the stack frame to look like an interrupt
 * return, then uses IRET to "return" to user mode.
 */

.section .text
.global enter_usermode
.type enter_usermode, @function

/* void enter_usermode(uint32_t entry_point, uint32_t user_stack)
 * 
 * Parameters:
 *   entry_point: User code address (where to jump in Ring 3)
 *   user_stack:  User stack pointer (top of user stack)
 * 
 * This function does NOT return - it jumps to user mode!
 */
enter_usermode:
    cli                         /* Disable interrupts during transition */
    
    /* Get parameters from stack */
    mov 4(%esp), %eax          /* entry_point */
    mov 8(%esp), %ecx          /* user_stack */
    
    /* Set up data segments for user mode (0x20 | 3 = 0x23) */
    mov $0x23, %bx
    mov %bx, %ds
    mov %bx, %es
    mov %bx, %fs
    mov %bx, %gs
    
    /* Build IRET stack frame */
    pushl $0x23                /* SS: User data segment */
    pushl %ecx                 /* ESP: User stack pointer */
    
    /* EFLAGS: Enable interrupts (IF=1), clear IOPL */
    pushfl
    pop %edx
    or $0x200, %edx            /* Set IF bit */
    and $0xFFFFCFFF, %edx      /* Clear IOPL bits */
    pushl %edx
    
    pushl $0x1B                /* CS: User code segment (0x18 | 3) */
    pushl %eax                 /* EIP: User entry point */
    
    /* Jump to user mode! */
    iret

.size enter_usermode, . - enter_usermode

.section .note.GNU-stack,"",@progbits
