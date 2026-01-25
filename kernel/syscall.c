/* kernel/syscall.c - System Call Handler
 *
 * UPDATED: Full implementations of fork, wait, exec
 */

#include "syscall.h"
#include "kernel.h"
#include "scheduler.h"
#include "task.h"
#include "elf.h"
#include "../fs/vfs.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"

/* ================================================================
 * HELPER FUNCTIONS
 * ================================================================ */

/* Clone a page directory for fork() */
static uint32_t *clone_page_directory(uint32_t *src_pd)
{
    /* Allocate new page directory - use PMM for page-aligned allocation */
    uint32_t *new_pd = (uint32_t *)pmm_alloc_block();
    if (!new_pd)
    {
        terminal_writestring("[FORK] Failed to allocate page directory\n");
        return NULL;
    }

    /* Clear it */
    memset(new_pd, 0, 4096);

    /* Copy kernel mappings (upper 1GB) - these are shared */
    for (int i = 768; i < 1024; i++)
    {
        new_pd[i] = src_pd[i];
    }

    /* Clone user space mappings (lower 3GB) */
    for (int i = 0; i < 768; i++)
    {
        if (!(src_pd[i] & 0x1))
            continue; /* Not present, skip */

        /* Get source page table */
        uint32_t *src_pt = (uint32_t *)(src_pd[i] & ~0xFFF);

        /* Allocate new page table - use PMM for page-aligned allocation */
        uint32_t *new_pt = (uint32_t *)pmm_alloc_block();
        if (!new_pt)
        {
            terminal_writestring("[FORK] Failed to allocate page table\n");
            /* TODO: proper cleanup on failure */
            return NULL;
        }

        /* Copy page table entries */
        for (int j = 0; j < 1024; j++)
        {
            if (!(src_pt[j] & 0x1))
                continue; /* Not present, skip */

            /* Allocate new physical page */
            uint32_t new_phys = (uint32_t)pmm_alloc_block();
            if (!new_phys)
            {
                terminal_writestring("[FORK] Failed to allocate physical page\n");
                /* TODO: proper cleanup */
                return NULL;
            }

            /* Get source physical page */
            uint32_t src_phys = src_pt[j] & ~0xFFF;

            /* Copy page contents - need to map both temporarily */
            /* For simplicity, assuming we can access them directly */
            memcpy((void *)new_phys, (void *)src_phys, 4096);

            /* Set page table entry with same flags */
            new_pt[j] = new_phys | (src_pt[j] & 0xFFF);
        }

        /* Set page directory entry with same flags */
        new_pd[i] = ((uint32_t)new_pt) | (src_pd[i] & 0xFFF);
    }

    return new_pd;
}

/* Note: Child's context is copied from parent's task structure in sys_fork(),
 * not from the interrupt registers. The child will resume with the same
 * context as the parent had when fork() was called. */

/* ================================================================
 * SYSCALL IMPLEMENTATIONS
 * ================================================================ */

void sys_exit(int code)
{
    if (!current_task || current_task == kernel_task)
    {
        return; /* Can't exit kernel */
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
    if ((uint32_t)msg >= 0xC0000000)
    {
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
    if (!current_task)
    {
        terminal_writestring("[FORK] ERROR: No current task\n");
        return -1;
    }

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[FORK] Parent PID ");
    terminal_write_dec(current_task->pid);
    terminal_writestring(" is forking...\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    /* Create child task structure */
    task_t *child = (task_t *)kmalloc(sizeof(task_t));
    if (!child)
    {
        terminal_writestring("[FORK] ERROR: Failed to allocate child task\n");
        return -1;
    }

    /* Copy parent's entire task structure */
    memcpy(child, current_task, sizeof(task_t));

    /* Assign new PID */
    static uint32_t next_fork_pid = 100; /* Start fork PIDs at 100 */
    child->pid = next_fork_pid++;

    /* Update name */
    strcat(child->name, "-child");

    /* Set up parent-child relationship */
    task_add_child(current_task, child);

    /* Clone page directory (deep copy of memory) */
    child->page_directory = clone_page_directory(current_task->page_directory);
    if (!child->page_directory)
    {
        terminal_writestring("[FORK] ERROR: Failed to clone page directory\n");
        kfree(child);
        return -1;
    }

    /* Allocate new kernel stack */
    child->kernel_stack = (uint32_t)kmalloc(4096);
    if (!child->kernel_stack)
    {
        terminal_writestring("[FORK] ERROR: Failed to allocate kernel stack\n");
        /* TODO: free page directory */
        kfree(child);
        return -1;
    }

    /* NOTE: User stack is already cloned as part of page directory */

    /* Child starts in READY state */
    child->state = TASK_READY;
    child->exit_code = 0;
    child->waited = false;
    child->first_child = NULL;
    child->next_sibling = NULL;

    /* CRITICAL: Set child's EAX to 0 so it knows it's the child */
    child->context.eax = 0;

    /* Add to scheduler queue */
    extern void scheduler_add_task(task_t * task);
    scheduler_add_task(child);

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[FORK] ✓ Created child PID ");
    terminal_write_dec(child->pid);
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    /* Return child PID to parent, 0 to child */
    return child->pid;
}

int sys_wait(int *status)
{
    if (!current_task)
    {
        return -1;
    }

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
    terminal_writestring("[WAIT] Parent PID ");
    terminal_write_dec(current_task->pid);
    terminal_writestring(" waiting for children...\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    /* Check if we have any children */
    if (!current_task->first_child)
    {
        terminal_writestring("[WAIT] No children to wait for\n");
        return -1; /* No children */
    }

    /* Look for zombie children */
    while (1)
    {
        task_t *child = current_task->first_child;

        while (child)
        {
            if (child->state == TASK_ZOMBIE && !child->waited)
            {
                /* Found a zombie child! */
                int pid = child->pid;
                int exit_code = child->exit_code;

                /* Mark as waited */
                child->waited = true;

                /* Copy exit status if pointer valid */
                if (status && (uint32_t)status < 0xC0000000)
                {
                    *status = exit_code;
                }

                terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                terminal_writestring("[WAIT] ✓ Parent ");
                terminal_write_dec(current_task->pid);
                terminal_writestring(" reaped child ");
                terminal_write_dec(pid);
                terminal_writestring(" (exit code: ");
                terminal_write_dec(exit_code);
                terminal_writestring(")\n");
                terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

                /* Remove from children list and clean up */
                task_remove_child(current_task, child);

                /* TODO: Free child's memory properly */
                /* For now, just mark it for cleanup */

                return pid;
            }

            child = child->next_sibling;
        }

        /* No zombie children found - block and wait */
        terminal_writestring("[WAIT] Blocking until child exits...\n");
        task_block();
        task_yield();

        /* When we wake up, check again */
    }
}

int sys_exec(const char *path)
{
    /* Validate pointer */
    if ((uint32_t)path >= 0xC0000000)
    {
        terminal_writestring("[EXEC] Invalid path pointer\n");
        return -1;
    }

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[EXEC] Loading program: ");
    terminal_writestring(path);
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    /* Open file */
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0)
    {
        terminal_writestring("[EXEC] File not found: ");
        terminal_writestring(path);
        terminal_writestring("\n");
        return -1;
    }

    /* Read file into buffer (max 64KB for now) */
    void *elf_data = kmalloc(65536);
    if (!elf_data)
    {
        vfs_close(fd);
        terminal_writestring("[EXEC] Out of memory\n");
        return -1;
    }

    int bytes_read = vfs_read(fd, elf_data, 65536);
    vfs_close(fd);

    if (bytes_read <= 0)
    {
        kfree(elf_data);
        terminal_writestring("[EXEC] Failed to read file\n");
        return -1;
    }

    terminal_writestring("[EXEC] Read ");
    terminal_write_dec(bytes_read);
    terminal_writestring(" bytes\n");

    /* DEBUG: Verify ELF data at offset 0x1000 */
    terminal_writestring("[EXEC_DEBUG] ELF magic: ");
    uint8_t *elf_bytes = (uint8_t *)elf_data;
    for (int i = 0; i < 4; i++) {
        terminal_write_hex(elf_bytes[i]);
        terminal_putchar(' ');
    }
    terminal_writestring("\n");

    terminal_writestring("[EXEC_DEBUG] Data at offset 0x1000: ");
    for (int i = 0; i < 8; i++) {
        terminal_write_hex(elf_bytes[0x1000 + i]);
        terminal_putchar(' ');
    }
    terminal_writestring("\n");

    /* If we're in kernel mode (shell), create a NEW user task */
    if (!current_task || current_task->ring == 0)
    {
        /* Create new user task */
        task_t *user_task = task_create_user(path, elf_data, 1);
        kfree(elf_data);

        if (!user_task)
        {
            terminal_writestring("[EXEC] Failed to create user task\n");
            return -1;
        }

        terminal_writestring("[EXEC] Task PID: ");
        terminal_write_dec(user_task->pid);
        terminal_writestring("\n");

        /* Add to scheduler */
        scheduler_add_task(user_task);

        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("[EXEC] ✓ Task added to scheduler\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

        /* Return success - caller can yield if desired */
        return 0;
    }

    /* If we're already in user mode, replace current process */
    /* Load ELF into current task's address space */
    if (!elf_load(current_task, elf_data))
    {
        kfree(elf_data);
        terminal_writestring("[EXEC] Invalid ELF file\n");
        return -1;
    }

    kfree(elf_data);

    terminal_writestring("[EXEC] ELF loaded, entry point: 0x");
    terminal_write_hex(current_task->entry_point);
    terminal_writestring("\n");

    /* Setup user context to jump to new entry point */
    task_setup_user_context(current_task);

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[EXEC] ✓ Ready to execute new program\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    /* Return 0 - but modified context means we'll jump to new program on syscall return */
    return 0;
}

/* ================================================================
 * SYSCALL DISPATCHER
 * ================================================================ */

void syscall_handler(struct registers *regs)
{
    /* Debug output (can be removed later) */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK));
    terminal_writestring("[SYSCALL] Number=");
    terminal_write_dec(regs->eax);
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    uint32_t syscall_num = regs->eax;

    /* Bounds check */
    if (syscall_num >= SYSCALL_MAX)
    {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("[SYSCALL] Invalid syscall number: ");
        terminal_write_dec(syscall_num);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        regs->eax = (uint32_t)-1;
        return;
    }

    /* Dispatch */
    switch (syscall_num)
    {
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
    {
        int child_pid = sys_fork();

        if (child_pid > 0)
        {
            /* Parent process */
            regs->eax = child_pid;
        }
        else if (child_pid == 0)
        {
            /* This shouldn't happen in parent */
            regs->eax = 0;
        }
        else
        {
            /* Error */
            regs->eax = (uint32_t)-1;
        }

        /* IMPORTANT: Child task needs EAX=0 when it starts */
        /* This will be set when the child is scheduled */
    }
    break;

    case SYS_EXEC:
        regs->eax = sys_exec((const char *)regs->ebx);
        break;

    case SYS_WAIT:
        regs->eax = sys_wait((int *)regs->ebx);
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
    terminal_writestring("[SYSCALL] Available: exit, write, read, yield, getpid, sleep, fork, exec, wait\n");
}