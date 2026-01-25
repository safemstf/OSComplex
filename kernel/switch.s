/* kernel/switch.s - Context Switching Assembly
 * 
 * CRITICAL: Offsets must match cpu_context_t and task_t in task.h!
 */

.section .text
.global task_switch_asm

.set CONTEXT_OFFSET, 44

/* void task_switch_asm(task_t *old_task, task_t *new_task) */
task_switch_asm:
    cli                         /* Disable interrupts during switch */
    
    /* Get parameters into registers we won't overwrite */
    mov 4(%esp), %eax          /* eax = old_task */
    mov 8(%esp), %edx          /* edx = new_task */
    
    /* Skip saving if old_task is NULL */
    test %eax, %eax
    jz .load_new
    
    /* ====== SAVE OLD TASK ====== */
    /* Save general purpose registers in order */
    mov %edi, CONTEXT_OFFSET+0(%eax)   /* edi */
    mov %esi, CONTEXT_OFFSET+4(%eax)   /* esi */
    mov %ebp, CONTEXT_OFFSET+8(%eax)   /* ebp */
    
    /* Save ESP: current stack pointer + 8 (skip return address and old param) */
    lea 8(%esp), %ecx
    mov %ecx, CONTEXT_OFFSET+12(%eax)  /* esp */
    
    mov %ebx, CONTEXT_OFFSET+16(%eax)  /* ebx */
    
    /* Save actual EDX value (not the parameter!) */
    push %edx                          /* Temporarily save new_task */
    mov 12(%esp), %edx                 /* Get original EDX from stack (before call) */
    mov %edx, CONTEXT_OFFSET+20(%eax)  /* Save it */
    pop %edx                           /* Restore new_task */
    
    mov %ecx, CONTEXT_OFFSET+24(%eax)  /* ecx */
    
    /* Save eax */
    push %eax                          /* Save old_task pointer */
    mov 8(%esp), %ecx                  /* Get original EAX from stack */
    pop %eax                           /* Restore old_task pointer */
    mov %ecx, CONTEXT_OFFSET+28(%eax)  /* Save original EAX */
    
    /* Save segment registers */
    mov %ds, %cx
    mov %cx, CONTEXT_OFFSET+32(%eax)   /* ds */
    mov %es, %cx
    mov %cx, CONTEXT_OFFSET+36(%eax)   /* es */
    mov %fs, %cx
    mov %cx, CONTEXT_OFFSET+40(%eax)   /* fs */
    mov %gs, %cx
    mov %cx, CONTEXT_OFFSET+44(%eax)   /* gs */
    
    /* Save return address as EIP */
    mov (%esp), %ecx
    mov %ecx, CONTEXT_OFFSET+48(%eax)  /* eip */
    
    /* Save CS and EFLAGS */
    mov %cs, %cx
    mov %cx, CONTEXT_OFFSET+52(%eax)   /* cs */
    pushf
    pop %ecx
    mov %ecx, CONTEXT_OFFSET+56(%eax)  /* eflags */
    
.load_new:
    /* ====== LOAD NEW TASK ====== */
    /* edx contains new_task */
    
    /* Load segment registers FIRST */
    mov CONTEXT_OFFSET+32(%edx), %bx
    mov %bx, %ds
    mov CONTEXT_OFFSET+36(%edx), %bx
    mov %bx, %es
    mov CONTEXT_OFFSET+40(%edx), %bx
    mov %bx, %fs
    mov CONTEXT_OFFSET+44(%edx), %bx
    mov %bx, %gs
    
    /* Load stack pointer */
    mov CONTEXT_OFFSET+12(%edx), %esp
    
    /* Load general purpose registers (except eax, edx) */
    mov CONTEXT_OFFSET+0(%edx), %edi
    mov CONTEXT_OFFSET+4(%edx), %esi
    mov CONTEXT_OFFSET+8(%edx), %ebp
    mov CONTEXT_OFFSET+16(%edx), %ebx
    mov CONTEXT_OFFSET+24(%edx), %ecx
    
    /* Push return address (EIP) */
    push CONTEXT_OFFSET+48(%edx)
    
    /* Load EFLAGS */
    push CONTEXT_OFFSET+56(%edx)
    popf
    
    /* Load EAX first, then EDX last (destroys pointer) */
    mov CONTEXT_OFFSET+28(%edx), %eax
    mov CONTEXT_OFFSET+20(%edx), %edx
    
    sti                         /* Re-enable interrupts */
    ret                         /* Jump to saved EIP */

.section .note.GNU-stack,"",@progbits