/* kernel/syscall.h - System Call Interface
 * 
 * Provides the interface between user programs and kernel.
 * User programs trigger INT 0x80 with syscall number in EAX.
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

/* Forward declaration - struct registers defined in kernel.h */
struct registers;

/* ================================================================
 * SYSTEM CALL NUMBERS
 * ================================================================ */

#define SYS_EXIT    0   /* exit(int code) */
#define SYS_WRITE   1   /* write(const char* msg) */
#define SYS_READ    2   /* read(char* buf, size_t len) */
#define SYS_YIELD   3   /* yield() - give up CPU */
#define SYS_GETPID  4   /* getpid() - get process ID */
#define SYS_SLEEP   5   /* sleep(uint32_t ms) */
#define SYS_FORK    6   /* fork() - create child process */
#define SYS_EXEC    7   /* exec(void* entry) - replace current process */

#define SYSCALL_MAX 8

/* ================================================================
 * SYSTEM CALL INTERFACE
 * ================================================================ */

/* Initialize system call handler */
void syscall_init(void);

/* Defined in syscall.s */
extern void syscall_stub(void);

/* System call handler (called from assembly) */
void syscall_handler(struct registers *regs);

/* ================================================================
 * SYSTEM CALL IMPLEMENTATIONS
 * ================================================================ */

/* Exit current process */
void sys_exit(int code);

/* Write string to terminal */
int sys_write(const char *msg);

/* Read input (placeholder) */
int sys_read(char *buf, size_t len);

/* Yield CPU to another task */
void sys_yield(void);

/* Get current process ID */
uint32_t sys_getpid(void);

/* Sleep for milliseconds */
void sys_sleep(uint32_t ms);

/* Fork - create child process (placeholder) */
int sys_fork(void);

/* Execute - replace current process (placeholder) */
int sys_exec(void *entry);

#endif /* SYSCALL_H */