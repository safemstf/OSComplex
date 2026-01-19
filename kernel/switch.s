/* kernel/switch.s - Context Switching Assembly
 * 
 * Switches execution from one task to another.
 * 
 * CRITICAL: Offsets must match cpu_context_t and task_t in task.h!
 * 
 * task_t layout:
 * - pid (4 bytes)
 * - name[32] (32 bytes)
 * - state (4 bytes)
 * - priority (4 bytes)
 * - context (cpu_context_t) starts at offset 44
 */

.section .text
.global task_switch_asm

.set CONTEXT_OFFSET, 44

/* void task_switch_asm(task_t *old_task, task_t *new_task) */
task_switch_asm:
    cli                         /* Disable interrupts during switch */
    
    mov 4(%esp), %eax          /* eax = old_task */
    mov 8(%esp), %ecx          /* ecx = new_task */
    
    /* Skip saving if old_task is NULL */
    test %eax, %eax
    jz load_new
    
    /* ====== SAVE OLD TASK ====== */
    /* Save general purpose registers to old_task->context */
    mov %edi, CONTEXT_OFFSET+0(%eax)
    mov %esi, CONTEXT_OFFSET+4(%eax)
    mov %ebp, CONTEXT_OFFSET+8(%eax)
    mov %esp, CONTEXT_OFFSET+12(%eax)
    mov %ebx, CONTEXT_OFFSET+16(%eax)
    mov %edx, CONTEXT_OFFSET+20(%eax)
    
    /* Save ecx (currently holds new_task, get original from stack) */
    push %ecx
    mov 8(%esp), %ecx
    mov %ecx, CONTEXT_OFFSET+24(%eax)
    pop %ecx
    
    /* Save eax (get original from before function call) */
    push %ecx
    mov 12(%esp), %edx
    mov %edx, CONTEXT_OFFSET+28(%eax)
    pop %ecx
    
    /* Save segment registers */
    mov %ds, %dx
    mov %dx, CONTEXT_OFFSET+32(%eax)
    mov %es, %dx
    mov %dx, CONTEXT_OFFSET+36(%eax)
    mov %fs, %dx
    mov %dx, CONTEXT_OFFSET+40(%eax)
    mov %gs, %dx
    mov %dx, CONTEXT_OFFSET+44(%eax)
    
    /* Save return address as EIP */
    mov (%esp), %edx
    mov %edx, CONTEXT_OFFSET+48(%eax)
    
    /* Save code segment and flags */
    mov %cs, %dx
    mov %dx, CONTEXT_OFFSET+52(%eax)
    pushf
    pop %edx
    mov %edx, CONTEXT_OFFSET+56(%eax)
    
load_new:
    /* ====== LOAD NEW TASK ====== */
    /* Restore segments */
    mov CONTEXT_OFFSET+32(%ecx), %dx
    mov %dx, %ds
    mov CONTEXT_OFFSET+36(%ecx), %dx
    mov %dx, %es
    mov CONTEXT_OFFSET+40(%ecx), %dx
    mov %dx, %fs
    mov CONTEXT_OFFSET+44(%ecx), %dx
    mov %dx, %gs
    
    /* Restore stack pointer */
    mov CONTEXT_OFFSET+12(%ecx), %esp
    
    /* Restore registers */
    mov CONTEXT_OFFSET+0(%ecx), %edi
    mov CONTEXT_OFFSET+4(%ecx), %esi
    mov CONTEXT_OFFSET+8(%ecx), %ebp
    mov CONTEXT_OFFSET+16(%ecx), %ebx
    mov CONTEXT_OFFSET+20(%ecx), %edx
    
    /* Push return address */
    push CONTEXT_OFFSET+48(%ecx)
    
    /* Restore flags */
    push CONTEXT_OFFSET+56(%ecx)
    popf
    
    /* Restore ecx and eax */
    mov CONTEXT_OFFSET+24(%ecx), %eax
    push %eax
    mov CONTEXT_OFFSET+28(%ecx), %eax
    pop %ecx
    
    sti                         /* Re-enable interrupts */
    ret                         /* Jump to saved EIP */

.section .note.GNU-stack,"",@progbits
