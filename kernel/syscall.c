/* kernel/syscall.c - System Call Implementation */

#include "syscall.h"
#include "kernel.h"
#include "task.h"
#include "scheduler.h"
#include "../drivers/terminal.h"

/* ================================================================
 * SYSCALL DISPATCH TYPE
 * ================================================================ */

/* All syscall entry points MUST share this signature */
typedef int (*syscall_fn_t)(
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t
);

static syscall_fn_t syscall_table[SYSCALL_MAX];

/* ================================================================
 * REAL SYSCALL IMPLEMENTATIONS
 * ================================================================ */

void sys_exit(int code)
{
    task_exit(code);
}

int sys_write(const char *msg)
{
    if (!msg)
        return -1;

    terminal_writestring(msg);
    return 0;
}

int sys_read(char *buf, size_t len)
{
    (void)buf;
    (void)len;
    return -1;
}

void sys_yield(void)
{
    task_yield();
}

uint32_t sys_getpid(void)
{
    task_t *current = task_current();
    return current ? current->pid : 0;
}

void sys_sleep(uint32_t ms)
{
    task_sleep(ms);
}

int sys_fork(void)
{
    terminal_writestring("[SYSCALL] fork() not implemented\n");
    return -1;
}

int sys_exec(void *entry)
{
    (void)entry;
    terminal_writestring("[SYSCALL] exec() not implemented\n");
    return -1;
}

/* ================================================================
 * SYSCALL ENTRY WRAPPERS (ABI SAFE)
 * ================================================================ */

static int sys_exit_entry(uint32_t code, uint32_t a2, uint32_t a3,
                          uint32_t a4, uint32_t a5)
{
    (void)a2; (void)a3; (void)a4; (void)a5;
    sys_exit((int)code);
    return 0;
}

static int sys_write_entry(uint32_t msg, uint32_t a2, uint32_t a3,
                           uint32_t a4, uint32_t a5)
{
    (void)a2; (void)a3; (void)a4; (void)a5;
    return sys_write((const char *)msg);
}

static int sys_read_entry(uint32_t buf, uint32_t len, uint32_t a3,
                          uint32_t a4, uint32_t a5)
{
    (void)a3; (void)a4; (void)a5;
    return sys_read((char *)buf, (size_t)len);
}

static int sys_yield_entry(uint32_t a1, uint32_t a2, uint32_t a3,
                           uint32_t a4, uint32_t a5)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    sys_yield();
    return 0;
}

static int sys_getpid_entry(uint32_t a1, uint32_t a2, uint32_t a3,
                            uint32_t a4, uint32_t a5)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return (int)sys_getpid();
}

static int sys_sleep_entry(uint32_t ms, uint32_t a2, uint32_t a3,
                           uint32_t a4, uint32_t a5)
{
    (void)a2; (void)a3; (void)a4; (void)a5;
    sys_sleep(ms);
    return 0;
}

static int sys_fork_entry(uint32_t a1, uint32_t a2, uint32_t a3,
                          uint32_t a4, uint32_t a5)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return sys_fork();
}

static int sys_exec_entry(uint32_t entry, uint32_t a2, uint32_t a3,
                          uint32_t a4, uint32_t a5)
{
    (void)a2; (void)a3; (void)a4; (void)a5;
    return sys_exec((void *)entry);
}

/* ================================================================
 * SYSCALL HANDLER
 * ================================================================ */

void syscall_handler(struct registers *regs)
{
    uint32_t num = regs->eax;

    if (num >= SYSCALL_MAX || !syscall_table[num]) {
        regs->eax = (uint32_t)-1;
        return;
    }

    regs->eax = syscall_table[num](
        regs->ebx,
        regs->ecx,
        regs->edx,
        regs->esi,
        regs->edi
    );
}

/* ================================================================
 * INITIALIZATION
 * ================================================================ */

void syscall_init(void)
{
    for (int i = 0; i < SYSCALL_MAX; i++)
        syscall_table[i] = NULL;

    syscall_table[SYS_EXIT]   = sys_exit_entry;
    syscall_table[SYS_WRITE]  = sys_write_entry;
    syscall_table[SYS_READ]   = sys_read_entry;
    syscall_table[SYS_YIELD]  = sys_yield_entry;
    syscall_table[SYS_GETPID] = sys_getpid_entry;
    syscall_table[SYS_SLEEP]  = sys_sleep_entry;
    syscall_table[SYS_FORK]   = sys_fork_entry;
    syscall_table[SYS_EXEC]   = sys_exec_entry;

    idt_set_gate(0x80, (uint32_t)syscall_stub, 0x08, 0xEE);

    terminal_writestring("[SYSCALL] INT 0x80 initialized\n");
}
