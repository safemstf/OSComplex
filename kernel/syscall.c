/* kernel/syscall.c - System Call Handler
 * 
 * CLEANED: Proper implementations, no duplicate code
 */

#include "syscall.h"
#include "kernel.h"
#include "scheduler.h"
#include "task.h"
#include "elf.h"
#include "../fs/vfs.h"

/* ================================================================
 * SYSCALL IMPLEMENTATIONS
 * ================================================================ */

void sys_exit(int code)
{
    if (!current_task || current_task == kernel_task) {
        return;  /* Can't exit kernel */
    }
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("\n[EXIT] Process ");
    terminal_write_dec(current_task->pid);
    terminal_writestring(" (");
    terminal_writestring(current_task->name);
    terminal_writestring(") exited with code ");
    terminal_write_dec(code);
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    /* Mark as zombie and yield */
    task_exit(code);
    
    /* Never returns */
}

int sys_write(const char *msg)
{
    /* Validate pointer is in user space (< 0xC0000000) */
    if ((uint32_t)msg >= 0xC0000000) {
        return -1;
    }
    
    terminal_writestring(msg);
    return 0;
}

int sys_read(char *buf, size_t len)
{
    /* TODO: Implement keyboard read */
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
    return current_task ? current_task->pid : 0;
}

void sys_sleep(uint32_t ms)
{
    task_sleep(ms);
}

int sys_fork(void)
{
    /* TODO: Implement fork */
    return -1;
}

int sys_exec(const char *path)
{
    /* Validate pointer */
    if ((uint32_t)path >= 0xC0000000) {
        return -1;
    }
    
    /* Open file */
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        terminal_writestring("[EXEC] File not found: ");
        terminal_writestring(path);
        terminal_writestring("\n");
        return -1;
    }
    
    /* Get file info */
    /* TODO: Need vfs_stat or similar to get file size */
    /* For now, read max 64KB */
    void *elf_data = kmalloc(65536);
    if (!elf_data) {
        vfs_close(fd);
        return -1;
    }
    
    int bytes_read = vfs_read(fd, elf_data, 65536);
    vfs_close(fd);
    
    if (bytes_read <= 0) {
        kfree(elf_data);
        return -1;
    }
    
    /* Load ELF into current task's address space */
    if (!elf_load(current_task, elf_data)) {
        kfree(elf_data);
        terminal_writestring("[EXEC] Invalid ELF file\n");
        return -1;
    }
    
    kfree(elf_data);
    
    /* Setup user context to jump to new entry point */
    /* This will cause the task to start executing the new program */
    extern void task_setup_user_context(task_t *task);
    task_setup_user_context(current_task);
    
    /* Return 0 - but modified context means we'll jump to new program */
    return 0;
}

/* ================================================================
 * SYSCALL DISPATCHER
 * ================================================================ */

void syscall_handler(struct registers *regs)
{
    uint32_t syscall_num = regs->eax;
    
    /* Bounds check */
    if (syscall_num >= SYSCALL_MAX) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("[SYSCALL] Invalid syscall number: ");
        terminal_write_dec(syscall_num);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        regs->eax = (uint32_t)-1;
        return;
    }
    
    /* Dispatch */
    switch (syscall_num) {
        case SYS_EXIT:
            sys_exit((int)regs->ebx);
            /* Never returns */
            break;
            
        case SYS_WRITE:
            regs->eax = sys_write((const char *)regs->ebx);
            break;
            
        case SYS_READ:
            regs->eax = sys_read((char *)regs->ebx, regs->ecx);
            break;
            
        case SYS_YIELD:
            sys_yield();
            regs->eax = 0;
            break;
            
        case SYS_GETPID:
            regs->eax = sys_getpid();
            break;
            
        case SYS_SLEEP:
            sys_sleep(regs->ebx);
            regs->eax = 0;
            break;
            
        case SYS_FORK:
            regs->eax = sys_fork();
            break;
            
        case SYS_EXEC:
            regs->eax = sys_exec((const char *)regs->ebx);
            break;
            
        default:
            regs->eax = (uint32_t)-1;
            break;
    }
}

/* ================================================================
 * INITIALIZATION
 * ================================================================ */

void syscall_init(void)
{
    /* Install INT 0x80 - DPL=3 for user mode access */
    idt_set_gate(0x80, (uint32_t)syscall_stub, 0x08, 0xEE);
    
    terminal_writestring("[SYSCALL] System call interface initialized\n");
}
