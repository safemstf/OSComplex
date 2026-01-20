/* user/init.s - Simple User Mode Test Program
 * 
 * This is the first user program for OSComplex.
 * It runs in Ring 3 and makes syscalls to test the interface.
 */

.section .text
.global _start

_start:
    /* Write "Hello from user mode!" using SYS_WRITE */
    mov $1, %eax              /* SYS_WRITE */
    mov $message, %ebx        /* Message pointer */
    int $0x80                 /* Syscall */
    
    /* Exit with status 0 */
    mov $0, %eax              /* SYS_EXIT */
    mov $0, %ebx              /* Exit code 0 */
    int $0x80                 /* Syscall */
    
    /* Should never reach here */
    jmp .

.section .rodata
message:
    .asciz "Hello from user mode!\n"
    