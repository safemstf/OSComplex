/* user/test_user.c - First User Mode Program
 * 
 * This is a simple program that runs in Ring 3.
 * It uses syscalls to interact with the kernel.
 */

#include <stdint.h>

/* System call numbers (must match kernel/syscall.h) */
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  4

/* ====================================================================
 * SYSCALL WRAPPERS
 * ==================================================================== */

/* Make a syscall with no arguments */
static inline uint32_t syscall0(uint32_t syscall_num)
{
    uint32_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(syscall_num));
    return ret;
}

/* Make a syscall with one argument */
static inline uint32_t syscall1(uint32_t syscall_num, uint32_t arg1)
{
    uint32_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "b"(arg1));
    return ret;
}

/* ====================================================================
 * USER LIBRARY FUNCTIONS
 * ==================================================================== */

void user_write(const char *msg)
{
    syscall1(SYS_WRITE, (uint32_t)msg);
}

void user_exit(int code)
{
    syscall1(SYS_EXIT, (uint32_t)code);
    while(1);  /* Should never reach here */
}

uint32_t user_getpid(void)
{
    return syscall0(SYS_GETPID);
}

/* ====================================================================
 * USER PROGRAM ENTRY POINT
 * ==================================================================== */

void user_main(void)
{
    /* Print welcome message */
    user_write("Hello from User Mode (Ring 3)!\n");
    
    /* Get and display PID */
    uint32_t pid = user_getpid();
    user_write("My PID is: ");
    
    /* Simple number to string (no stdlib in user mode!) */
    char buf[16];
    int i = 0;
    if (pid == 0) {
        buf[i++] = '0';
    } else {
        uint32_t temp = pid;
        int digits = 0;
        while (temp > 0) {
            temp /= 10;
            digits++;
        }
        i = digits;
        temp = pid;
        while (temp > 0) {
            buf[--i] = '0' + (temp % 10);
            temp /= 10;
        }
        i = digits;
    }
    buf[i] = '\0';
    
    user_write(buf);
    user_write("\n");
    
    /* Test syscalls */
    user_write("Testing system calls:\n");
    user_write("  SYS_WRITE - works!\n");
    user_write("  SYS_GETPID - works!\n");
    user_write("  SYS_EXIT - testing now...\n");
    
    /* Exit cleanly */
    user_exit(42);
}