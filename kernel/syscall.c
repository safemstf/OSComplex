/* kernel/syscall.c - System Call Implementation
 * 
 * Handles system calls from user programs via INT 0x80.
 */

#include "syscall.h"
#include "kernel.h"
#include "task.h"
#include "scheduler.h"

/* Array of system call handlers */
typedef int (*syscall_fn_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
static syscall_fn_t syscall_table[SYSCALL_MAX];

/* ================================================================
 * SYSTEM CALL IMPLEMENTATIONS
 * ================================================================ */

void sys_exit(int code)
{
    task_exit(code);
}

int sys_write(const char *msg)
{
    /* Security: For now, trust the pointer (will validate later) */
    if (!msg) return -1;
    
    terminal_writestring(msg);
    return 0;
}

int sys_read(char *buf, size_t len)
{
    /* TODO: Implement keyboard input */
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
    /* TODO: Implement fork */
    terminal_writestring("[SYSCALL] fork() not implemented yet\n");
    return -1;
}

int sys_exec(void *entry)
{
    /* TODO: Implement exec */
    (void)entry;
    terminal_writestring("[SYSCALL] exec() not implemented yet\n");
    return -1;
}

/* ================================================================
 * SYSCALL HANDLER
 * ================================================================ */

void syscall_handler(struct registers *regs)
{
    /* System call number is in EAX */
    uint32_t syscall_num = regs->eax;
    
    /* Arguments in EBX, ECX, EDX, ESI, EDI */
    uint32_t arg1 = regs->ebx;
    uint32_t arg2 = regs->ecx;
    uint32_t arg3 = regs->edx;
    uint32_t arg4 = regs->esi;
    uint32_t arg5 = regs->edi;
    
    /* Validate syscall number */
    if (syscall_num >= SYSCALL_MAX) {
        terminal_writestring("[SYSCALL] Invalid syscall number: ");
        char buf[16];
        itoa(syscall_num, buf);
        terminal_writestring(buf);
        terminal_writestring("\n");
        regs->eax = -1;
        return;
    }
    
    /* Call the appropriate handler */
    syscall_fn_t handler = syscall_table[syscall_num];
    if (handler) {
        int result = handler(arg1, arg2, arg3, arg4, arg5);
        regs->eax = result;  /* Return value in EAX */
    } else {
        regs->eax = -1;
    }
}

/* ================================================================
 * INITIALIZATION
 * ================================================================ */

void syscall_init(void)
{
    /* Clear syscall table */
    for (int i = 0; i < SYSCALL_MAX; i++) {
        syscall_table[i] = NULL;
    }
    
    /* Register system calls */
    syscall_table[SYS_EXIT]   = (syscall_fn_t)sys_exit;
    syscall_table[SYS_WRITE]  = (syscall_fn_t)sys_write;
    syscall_table[SYS_READ]   = (syscall_fn_t)sys_read;
    syscall_table[SYS_YIELD]  = (syscall_fn_t)sys_yield;
    syscall_table[SYS_GETPID] = (syscall_fn_t)sys_getpid;
    syscall_table[SYS_SLEEP]  = (syscall_fn_t)sys_sleep;
    syscall_table[SYS_FORK]   = (syscall_fn_t)sys_fork;
    syscall_table[SYS_EXEC]   = (syscall_fn_t)sys_exec;
    
    /* Install INT 0x80 handler */
    idt_set_gate(0x80, (uint32_t)syscall_stub, 0x08, 0xEE);  /* DPL=3 for user mode */
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[SYSCALL] System call interface initialized (INT 0x80)\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}