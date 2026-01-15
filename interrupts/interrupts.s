/* interrupts/interrupts.s - Low-level interrupt handler stubs (FIXED)
 * 
 * FIX APPLIED: Replaced IRQ macro with manual stubs because macro
 * parameter substitution was failing (push $\irq_num not expanding correctly)
 */

.section .text

/* Macro: Create an ISR stub WITHOUT an error code */
.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    cli
    push $0
    push $\num
    jmp isr_common_stub
.endm

/* Macro: Create an ISR stub WITH an error code */
.macro ISR_ERRCODE num
.global isr\num
isr\num:
    cli
    push $\num
    jmp isr_common_stub
.endm

/* CPU EXCEPTION HANDLERS (0-31) - These work fine with macros */
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_ERRCODE   21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

/* ================================================================
 * HARDWARE INTERRUPT HANDLERS (IRQs 0-15)
 * ================================================================
 * MANUALLY WRITTEN - Macro expansion was failing!
 * Each IRQ stub explicitly pushes its interrupt number (32-47)
 */

.global irq0
irq0:
    cli
    push $0
    push $32        /* Timer interrupt */
    jmp irq_common_stub

.global irq1
irq1:
    cli
    push $0
    push $33        /* Keyboard interrupt */
    jmp irq_common_stub

.global irq2
irq2:
    cli
    push $0
    push $34        /* Cascade */
    jmp irq_common_stub

.global irq3
irq3:
    cli
    push $0
    push $35        /* COM2 */
    jmp irq_common_stub

.global irq4
irq4:
    cli
    push $0
    push $36        /* COM1 */
    jmp irq_common_stub

.global irq5
irq5:
    cli
    push $0
    push $37        /* LPT2 */
    jmp irq_common_stub

.global irq6
irq6:
    cli
    push $0
    push $38        /* Floppy disk */
    jmp irq_common_stub

.global irq7
irq7:
    cli
    push $0
    push $39        /* LPT1 */
    jmp irq_common_stub

.global irq8
irq8:
    cli
    push $0
    push $40        /* CMOS real-time clock */
    jmp irq_common_stub

.global irq9
irq9:
    cli
    push $0
    push $41        /* Free for peripherals */
    jmp irq_common_stub

.global irq10
irq10:
    cli
    push $0
    push $42        /* Free for peripherals */
    jmp irq_common_stub

.global irq11
irq11:
    cli
    push $0
    push $43        /* Free for peripherals */
    jmp irq_common_stub

.global irq12
irq12:
    cli
    push $0
    push $44        /* PS/2 Mouse */
    jmp irq_common_stub

.global irq13
irq13:
    cli
    push $0
    push $45        /* FPU / Coprocessor */
    jmp irq_common_stub

.global irq14
irq14:
    cli
    push $0
    push $46        /* Primary ATA hard disk */
    jmp irq_common_stub

.global irq15
irq15:
    cli
    push $0
    push $47        /* Secondary ATA hard disk */
    jmp irq_common_stub

/* ================================================================
 * COMMON ISR HANDLER
 * ================================================================ */
isr_common_stub:
    pusha
    
    push %ds
    push %es
    push %fs
    push %gs
    
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    call isr_handler
    
    pop %gs
    pop %fs
    pop %es
    pop %ds
    
    popa
    
    add $8, %esp
    
    iret

/* ================================================================
 * COMMON IRQ HANDLER
 * ================================================================ */
irq_common_stub:
    pusha
    
    push %ds
    push %es
    push %fs
    push %gs
    
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    call irq_handler
    
    pop %gs
    pop %fs
    pop %es
    pop %ds
    
    popa
    
    add $8, %esp
    
    iret

.section .note.GNU-stack,"",@progbits