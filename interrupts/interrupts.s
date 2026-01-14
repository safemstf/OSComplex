/* interrupts.s - Low-level interrupt handler stubs
 * 
 * WHY WE NEED ASSEMBLY HERE:
 * When an interrupt occurs, the CPU needs to:
 * 1. Save the current state (registers)
 * 2. Call the handler
 * 3. Restore the state
 * 4. Return from interrupt (special instruction: IRET)
 * 
 * We can't do this purely in C because:
 * - C doesn't have direct access to CPU registers
 * - C doesn't know about the IRET instruction
 * - We need precise control over the stack
 * 
 * ARCHITECTURE:
 * These stubs are the "glue" between hardware and software.
 * Hardware -> Assembly stub -> C handler function
 * 
 * Each stub does:
 * 1. Save all registers (PUSHA)
 * 2. Call the C handler
 * 3. Restore registers (POPA)
 * 4. Return from interrupt (IRET)
 */

.section .text

/* Macro: Create an ISR stub WITHOUT an error code
 * 
 * Most CPU exceptions don't push an error code. We push a dummy
 * 0 to keep the stack frame consistent.
 */
.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    cli                    /* Disable interrupts */
    push $0                /* Push dummy error code */
    push $\num             /* Push interrupt number */
    jmp isr_common_stub    /* Jump to common handler */
.endm

/* Macro: Create an ISR stub WITH an error code
 * 
 * Some CPU exceptions (like page fault, GPF) push an error code
 * automatically. We don't need to push a dummy one.
 */
.macro ISR_ERRCODE num
.global isr\num
isr\num:
    cli                    /* Disable interrupts */
    push $\num             /* Push interrupt number */
    jmp isr_common_stub    /* Jump to common handler */
.endm

/* Macro: Create an IRQ stub
 * 
 * Hardware interrupts (IRQs) need to:
 * 1. Save state
 * 2. Call handler
 * 3. Send EOI (End Of Interrupt) to PIC
 * 4. Restore state
 */
.macro IRQ num, irq_num
.global irq\num
irq\num:
    cli                    /* Disable interrupts */
    push $0                /* Push dummy error code */
    push $\irq_num         /* Push IRQ number */
    jmp irq_common_stub    /* Jump to common handler */
.endm

/* ================================================================
 * CPU EXCEPTION HANDLERS (0-31)
 * ================================================================
 * These are defined by Intel. Each has a specific meaning:
 */

ISR_NOERRCODE 0    /* Divide by zero */
ISR_NOERRCODE 1    /* Debug exception */
ISR_NOERRCODE 2    /* Non-maskable interrupt */
ISR_NOERRCODE 3    /* Breakpoint */
ISR_NOERRCODE 4    /* Overflow */
ISR_NOERRCODE 5    /* Bound range exceeded */
ISR_NOERRCODE 6    /* Invalid opcode */
ISR_NOERRCODE 7    /* Device not available (FPU) */
ISR_ERRCODE   8    /* Double fault (has error code) */
ISR_NOERRCODE 9    /* Coprocessor segment overrun */
ISR_ERRCODE   10   /* Invalid TSS (has error code) */
ISR_ERRCODE   11   /* Segment not present (has error code) */
ISR_ERRCODE   12   /* Stack segment fault (has error code) */
ISR_ERRCODE   13   /* General protection fault (has error code) */
ISR_ERRCODE   14   /* Page fault (has error code) */
ISR_NOERRCODE 15   /* Reserved */
ISR_NOERRCODE 16   /* x87 FPU error */
ISR_ERRCODE   17   /* Alignment check (has error code) */
ISR_NOERRCODE 18   /* Machine check */
ISR_NOERRCODE 19   /* SIMD floating point exception */
ISR_NOERRCODE 20   /* Virtualization exception */
ISR_ERRCODE   21   /* Control protection exception (has error code) */
ISR_NOERRCODE 22   /* Reserved */
ISR_NOERRCODE 23   /* Reserved */
ISR_NOERRCODE 24   /* Reserved */
ISR_NOERRCODE 25   /* Reserved */
ISR_NOERRCODE 26   /* Reserved */
ISR_NOERRCODE 27   /* Reserved */
ISR_NOERRCODE 28   /* Reserved */
ISR_NOERRCODE 29   /* Reserved */
ISR_NOERRCODE 30   /* Reserved */
ISR_NOERRCODE 31   /* Reserved */

/* ================================================================
 * HARDWARE INTERRUPT HANDLERS (IRQs 0-15)
 * ================================================================
 * These are remapped to interrupts 32-47 to avoid conflicts
 * with CPU exceptions.
 */

IRQ 0, 32    /* Timer interrupt */
IRQ 1, 33    /* Keyboard interrupt */
IRQ 2, 34    /* Cascade (used internally by PIC) */
IRQ 3, 35    /* COM2 */
IRQ 4, 36    /* COM1 */
IRQ 5, 37    /* LPT2 */
IRQ 6, 38    /* Floppy disk */
IRQ 7, 39    /* LPT1 */
IRQ 8, 40    /* CMOS real-time clock */
IRQ 9, 41    /* Free for peripherals / legacy SCSI / NIC */
IRQ 10, 42   /* Free for peripherals / SCSI / NIC */
IRQ 11, 43   /* Free for peripherals / SCSI / NIC */
IRQ 12, 44   /* PS/2 Mouse */
IRQ 13, 45   /* FPU / Coprocessor / Inter-processor */
IRQ 14, 46   /* Primary ATA hard disk */
IRQ 15, 47   /* Secondary ATA hard disk */

/* ================================================================
 * COMMON ISR HANDLER
 * ================================================================
 * All CPU exception stubs jump here. This code:
 * 1. Saves all registers
 * 2. Calls C handler (isr_handler)
 * 3. Restores registers
 * 4. Returns from interrupt
 * 
 * Stack layout when we get here (pushed by CPU/stub):
 * [SS]           <- Only if privilege level changed
 * [ESP]          <- Only if privilege level changed
 * EFLAGS
 * CS
 * EIP
 * Error code     <- Or dummy 0
 * Interrupt num
 */
isr_common_stub:
    /* Save all general-purpose registers
     * PUSHA pushes: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI */
    pusha
    
    /* Save segment registers
     * We need to preserve these too */
    push %ds
    push %es
    push %fs
    push %gs
    
    /* Load kernel data segment
     * Ensure we're using kernel's data segment for the handler */
    mov $0x10, %ax    /* 0x10 = kernel data segment selector */
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    /* Call C exception handler
     * The C function can access interrupt number and error code
     * from the stack */
    call isr_handler
    
    /* Restore segment registers */
    pop %gs
    pop %fs
    pop %es
    pop %ds
    
    /* Restore general-purpose registers */
    popa
    
    /* Clean up error code and interrupt number from stack
     * We pushed 2 values (8 bytes total) */
    add $8, %esp
    
    /* Return from interrupt
     * IRET pops: EIP, CS, EFLAGS (and ESP, SS if privilege changed)
     * This is a special instruction that returns from an interrupt */
    iret

/* ================================================================
 * COMMON IRQ HANDLER
 * ================================================================
 * All hardware interrupt stubs jump here. Similar to ISR handler,
 * but also needs to send EOI (End Of Interrupt) to the PIC.
 * 
 * The PIC won't send more interrupts until we acknowledge the
 * current one by sending EOI.
 */
irq_common_stub:
    /* Save all registers (same as ISR) */
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
    
    /* Call C IRQ handler
     * This will dispatch to the appropriate handler
     * (keyboard, timer, etc.) */
    call irq_handler
    
    /* Restore segments */
    pop %gs
    pop %fs
    pop %es
    pop %ds
    
    /* Restore registers */
    popa
    
    /* Clean up pushed values */
    add $8, %esp
    
    /* Return from interrupt */
    iret

/* Mark stack as non-executable */
.section .note.GNU-stack,"",@progbits