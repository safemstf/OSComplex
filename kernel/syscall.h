/* kernel/syscall.h - System Call Interface */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

/* Forward declaration */
struct registers;

/* ================================================================
 * SYSTEM CALL NUMBERS
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

#define SYSCALL_MAX 9

/* ================================================================
 * INITIALIZATION
 * ================================================================ */

void syscall_init(void);

/* Assembly entry point */
extern void syscall_stub(void);

/* C-level handler */
void syscall_handler(struct registers *regs);

/* ================================================================
 * INTERNAL SYSCALL IMPLEMENTATIONS
 * (real logic, NOT directly dispatched)
 * ================================================================ */

void     sys_exit(int code);
int      sys_write(const char *msg);
int      sys_read(char *buf, size_t len);
void     sys_yield(void);
uint32_t sys_getpid(void);
void     sys_sleep(uint32_t ms);
int      sys_fork(void);
int      sys_exec(const char *path);
int      sys_wait(int *status);

#endif /* SYSCALL_H */