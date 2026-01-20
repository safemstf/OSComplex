/* kernel/gdt_flush.s - Load GDT and Reload Segment Registers */

.section .text
.global gdt_flush

gdt_flush:
    mov 4(%esp), %eax
    lgdt (%eax)
    
    ljmp $0x08, $reload_segments
    
reload_segments:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    
    ret

.section .note.GNU-stack,"",@progbits
