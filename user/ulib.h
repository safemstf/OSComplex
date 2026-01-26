/* user/ulib.h - User Library Header
 *
 * Common definitions and syscall wrappers for user programs.
 * Include this in all user programs.
 */

#ifndef ULIB_H
#define ULIB_H

/* ================================================================
 * SYSCALL NUMBERS (must match kernel/syscall.c)
 * ================================================================ */
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_YIELD   3
#define SYS_GETPID  4
#define SYS_SLEEP   5
#define SYS_FORK    6
#define SYS_EXEC    7
#define SYS_WAIT    8
#define SYS_OPEN    9
#define SYS_CLOSE   10
#define SYS_FREAD   11

/* ================================================================
 * SYSCALL WRAPPERS
 * ================================================================ */

static inline int syscall0(int num)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num));
    return ret;
}

static inline int syscall1(int num, int arg1)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1));
    return ret;
}

static inline int syscall2(int num, int arg1, int arg2)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2));
    return ret;
}

static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3));
    return ret;
}

/* ================================================================
 * LIBRARY FUNCTIONS
 * ================================================================ */

/* Exit the program */
static inline void exit(int code)
{
    syscall1(SYS_EXIT, code);
    while (1) __asm__ volatile("hlt");
}

/* Write a string to stdout */
static inline int write(const char *str)
{
    return syscall1(SYS_WRITE, (int)str);
}

/* Get current process ID */
static inline int getpid(void)
{
    return syscall0(SYS_GETPID);
}

/* Yield CPU to other processes */
static inline void yield(void)
{
    syscall0(SYS_YIELD);
}

/* Sleep for ms milliseconds */
static inline void sleep(int ms)
{
    syscall1(SYS_SLEEP, ms);
}

/* Fork a new process */
static inline int fork(void)
{
    return syscall0(SYS_FORK);
}

/* Wait for child process */
static inline int wait(int *status)
{
    return syscall1(SYS_WAIT, (int)status);
}

/* Execute a program */
static inline int exec(const char *path)
{
    return syscall1(SYS_EXEC, (int)path);
}

/* ================================================================
 * STRING UTILITIES
 * ================================================================ */

/* Get string length */
static inline int strlen(const char *s)
{
    int len = 0;
    while (s[len]) len++;
    return len;
}

/* Print a number */
static inline void print_num(int n)
{
    char buf[12];
    int i = 0;
    int negative = 0;

    if (n == 0) {
        write("0");
        return;
    }

    if (n < 0) {
        negative = 1;
        n = -n;
    }

    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    if (negative) {
        buf[i++] = '-';
    }

    /* Print in reverse */
    while (i > 0) {
        char c[2] = {buf[--i], '\0'};
        write(c);
    }
}

/* Print a hex number */
static inline void print_hex(unsigned int n)
{
    char hex[] = "0123456789ABCDEF";
    char buf[9];
    int i;

    write("0x");
    for (i = 7; i >= 0; i--) {
        buf[i] = hex[n & 0xF];
        n >>= 4;
    }
    buf[8] = '\0';
    write(buf);
}

#endif /* ULIB_H */
