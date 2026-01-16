/* boot/boot.s - Multiboot bootloader for OSComplex
 * Provides entry point and stack setup for kernel initialization
 */

.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide memory map */
.set FLAGS,    ALIGN | MEMINFO  /* multiboot flags */
.set MAGIC,    0x1BADB002       /* magic number for bootloader */
.set CHECKSUM, -(MAGIC + FLAGS) /* checksum for verification */

/* Multiboot header - must be in first 8KB of kernel */
.section .multiboot, "a"
.align 4
multiboot_header:
.long MAGIC
.long FLAGS
.long CHECKSUM

/* Kernel entry point - immediately after multiboot header */
.align 4
.global _start
.type _start, @function
_start:
    /* Set up stack */
    mov $stack_top, %esp
    
    /* Reset EFLAGS */
    pushl $0
    popf
    
    /* Push multiboot magic and info pointer for kernel_main */
    pushl %ebx  /* Multiboot info structure */
    pushl %eax  /* Multiboot magic number */
    
    /* Jump to C kernel */
    call kernel_main
    
    /* If kernel returns, halt */
    cli
1:  hlt
    jmp 1b

.size _start, . - _start

/* Kernel stack (16KB) */
.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

/* Mark stack as non-executable (eliminates linker warning) */
.section .note.GNU-stack,"",@progbits
