/* kernel/tss_flush.s - Load TSS into Task Register */

.section .text
.global tss_flush

tss_flush:
    mov $0x28, %ax
    ltr %ax
    ret

.section .note.GNU-stack,"",@progbits
