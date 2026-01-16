/* interrupts/interrupts.s - Low-level interrupt handler stubs (FULLY FIXED)
 * 
 * FIX: ALL stubs manually written to avoid ANY macro expansion issues.
 * The bug was likely ISR stubs jumping to irq_common_stub instead of isr_common_stub.
 */

.section .text

/* ================================================================
 * CPU EXCEPTION HANDLERS (0-31) - MANUALLY WRITTEN
 * ================================================================
 * NO MACROS - explicit code for each handler to ensure correctness
 */

/* ISR 0: Divide by zero - NO error code */
.global isr0
isr0:
    cli
    push $0                /* dummy error code */
    push $0                /* interrupt number */
    jmp isr_common_stub

/* ISR 1: Debug - NO error code */
.global isr1
isr1:
    cli
    push $0
    push $1
    jmp isr_common_stub

/* ISR 2: Non-maskable interrupt - NO error code */
.global isr2
isr2:
    cli
    push $0
    push $2
    jmp isr_common_stub

/* ISR 3: Breakpoint - NO error code */
.global isr3
isr3:
    cli
    push $0
    push $3
    jmp isr_common_stub

/* ISR 4: Overflow - NO error code */
.global isr4
isr4:
    cli
    push $0
    push $4
    jmp isr_common_stub

/* ISR 5: Bound range exceeded - NO error code */
.global isr5
isr5:
    cli
    push $0
    push $5
    jmp isr_common_stub

/* ISR 6: Invalid opcode - NO error code */
.global isr6
isr6:
    cli
    push $0
    push $6
    jmp isr_common_stub

/* ISR 7: Device not available - NO error code */
.global isr7
isr7:
    cli
    push $0
    push $7
    jmp isr_common_stub

/* ISR 8: Double fault - HAS error code (CPU pushes it) */
.global isr8
isr8:
    cli
    /* CPU already pushed error code */
    push $8
    jmp isr_common_stub

/* ISR 9: Coprocessor segment overrun - NO error code */
.global isr9
isr9:
    cli
    push $0
    push $9
    jmp isr_common_stub

/* ISR 10: Invalid TSS - HAS error code */
.global isr10
isr10:
    cli
    push $10
    jmp isr_common_stub

/* ISR 11: Segment not present - HAS error code */
.global isr11
isr11:
    cli
    push $11
    jmp isr_common_stub

/* ISR 12: Stack segment fault - HAS error code */
.global isr12
isr12:
    cli
    push $12
    jmp isr_common_stub

/* ISR 13: General protection fault - HAS error code */
.global isr13
isr13:
    cli
    push $13
    jmp isr_common_stub

/* ISR 14: Page fault - HAS error code */
.global isr14
isr14:
    cli
    push $14
    jmp isr_common_stub

/* ISR 15: Reserved - NO error code */
.global isr15
isr15:
    cli
    push $0
    push $15
    jmp isr_common_stub

/* ISR 16: x87 FPU error - NO error code */
.global isr16
isr16:
    cli
    push $0
    push $16
    jmp isr_common_stub     /* CRITICAL: Must be isr_common_stub, NOT irq_common_stub! */

/* ISR 17: Alignment check - HAS error code */
.global isr17
isr17:
    cli
    push $17
    jmp isr_common_stub

/* ISR 18: Machine check - NO error code */
.global isr18
isr18:
    cli
    push $0
    push $18
    jmp isr_common_stub

/* ISR 19: SIMD floating point - NO error code */
.global isr19
isr19:
    cli
    push $0
    push $19
    jmp isr_common_stub

/* ISR 20: Virtualization exception - NO error code */
.global isr20
isr20:
    cli
    push $0
    push $20
    jmp isr_common_stub

/* ISR 21: Control protection - HAS error code */
.global isr21
isr21:
    cli
    push $21
    jmp isr_common_stub

/* ISR 22-31: Reserved - NO error code */
.global isr22
isr22:
    cli
    push $0
    push $22
    jmp isr_common_stub

.global isr23
isr23:
    cli
    push $0
    push $23
    jmp isr_common_stub

.global isr24
isr24:
    cli
    push $0
    push $24
    jmp isr_common_stub

.global isr25
isr25:
    cli
    push $0
    push $25
    jmp isr_common_stub

.global isr26
isr26:
    cli
    push $0
    push $26
    jmp isr_common_stub

.global isr27
isr27:
    cli
    push $0
    push $27
    jmp isr_common_stub

.global isr28
isr28:
    cli
    push $0
    push $28
    jmp isr_common_stub

.global isr29
isr29:
    cli
    push $0
    push $29
    jmp isr_common_stub

.global isr30
isr30:
    cli
    push $0
    push $30
    jmp isr_common_stub

.global isr31
isr31:
    cli
    push $0
    push $31
    jmp isr_common_stub

/* ================================================================
 * HARDWARE INTERRUPT HANDLERS (IRQs 0-15) - Mapped to INT 32-47
 * ================================================================ */

.global irq0
irq0:
    cli
    push $0
    push $32        /* Timer */
    jmp irq_common_stub

.global irq1
irq1:
    cli
    push $0
    push $33        /* Keyboard */
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
    push $38        /* Floppy */
    jmp irq_common_stub

.global irq7
irq7:
    cli
    push $0
    push $39        /* LPT1 / Spurious */
    jmp irq_common_stub

.global irq8
irq8:
    cli
    push $0
    push $40        /* RTC */
    jmp irq_common_stub

.global irq9
irq9:
    cli
    push $0
    push $41
    jmp irq_common_stub

.global irq10
irq10:
    cli
    push $0
    push $42
    jmp irq_common_stub

.global irq11
irq11:
    cli
    push $0
    push $43
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
    push $45        /* FPU */
    jmp irq_common_stub

.global irq14
irq14:
    cli
    push $0
    push $46        /* Primary ATA */
    jmp irq_common_stub

.global irq15
irq15:
    cli
    push $0
    push $47        /* Secondary ATA */
    jmp irq_common_stub

/* ================================================================
 * ISR COMMON STUB - For CPU Exceptions (int 0-31)
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
    
    add $8, %esp        /* Remove error code and interrupt number */
    
    iret

/* ================================================================
 * IRQ COMMON STUB - For Hardware Interrupts (int 32-47)
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
    
    add $8, %esp        /* Remove error code and interrupt number */
    
    iret

/* Mark stack as non-executable */
.section .note.GNU-stack,"",@progbits
