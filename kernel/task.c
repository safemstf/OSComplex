/* kernel/task.c - Task Management Implementation
 * 
 * Implements process creation, destruction, and context switching.
 * This is the core of our multitasking OS!
 */

#include "task.h"
#include "kernel.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"

/* ================================================================
 * GLOBAL STATE
 * ================================================================ */

/* Current running task */
static task_t *current_task = NULL;

/* Next PID to assign */
static uint32_t next_pid = 1;

/* Kernel idle task (runs when nothing else can) */
task_t *kernel_task = NULL;

/* Task list head (all tasks) */
static task_t *task_list_head = NULL;

/* ================================================================
 * FORWARD DECLARATIONS
 * ================================================================ */
static void idle_task_entry(void);
static void task_setup_stack(task_t *task, void (*entry_point)(void));

/* ================================================================
 * INITIALIZATION
 * ================================================================ */

void task_init(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[TASK] Initializing task management...\n");
    
    /* Create kernel idle task */
    kernel_task = kmalloc(sizeof(task_t));
    if (!kernel_task) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("[TASK] ERROR: Failed to allocate kernel task!\n");
        return;
    }
    
    memset(kernel_task, 0, sizeof(task_t));
    
    kernel_task->pid = 0;
    strcpy(kernel_task->name, "kernel_idle");
    kernel_task->state = TASK_RUNNING;
    kernel_task->priority = 255;  /* Lowest priority */
    kernel_task->page_directory = vmm_current_as->page_dir;
    kernel_task->parent = NULL;
    kernel_task->next = NULL;
    
    /* Kernel task is currently running */
    current_task = kernel_task;
    task_list_head = kernel_task;
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[TASK] Kernel idle task created (PID 0)\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

/* ================================================================
 * TASK CREATION
 * ================================================================ */

task_t* task_create(const char *name, void (*entry_point)(void), uint32_t priority)
{
    /* Allocate task structure */
    task_t *task = kmalloc(sizeof(task_t));
    if (!task) {
        terminal_writestring("[TASK] ERROR: Failed to allocate task structure\n");
        return NULL;
    }
    
    memset(task, 0, sizeof(task_t));
    
    /* Set task properties */
    task->pid = next_pid++;
    strncpy(task->name, name, 31);
    task->name[31] = '\0';
    task->state = TASK_READY;
    task->priority = priority;
    task->time_slice = 10;  /* 10ms default */
    task->total_time = 0;
    task->exit_code = 0;
    
    /* Allocate kernel stack (4KB) */
    task->kernel_stack = (uint32_t)kmalloc(4096);
    if (!task->kernel_stack) {
        terminal_writestring("[TASK] ERROR: Failed to allocate kernel stack\n");
        kfree(task);
        return NULL;
    }
    
    /* Allocate user stack (4KB) */
    task->user_stack = (uint32_t)kmalloc(4096);
    if (!task->user_stack) {
        terminal_writestring("[TASK] ERROR: Failed to allocate user stack\n");
        kfree((void*)task->kernel_stack);
        kfree(task);
        return NULL;
    }
    
    /* Use kernel address space for now (no user space yet) */
    task->page_directory = kernel_task->page_directory;
    
    /* Setup initial stack and context */
    task_setup_stack(task, entry_point);
    
    /* Set parent to current task */
    task->parent = current_task;
    
    /* Add to task list */
    task->next = task_list_head;
    task_list_head = task;
    
    /* Debug output */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[TASK] Created task '");
    terminal_writestring(task->name);
    terminal_writestring("' (PID ");
    char buf[16];
    itoa(task->pid, buf);
    terminal_writestring(buf);
    terminal_writestring(")\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    return task;
}

/* ================================================================
 * STACK SETUP
 * Setup initial stack so task can start executing
 * ================================================================ */

static void task_setup_stack(task_t *task, void (*entry_point)(void))
{
    /* Stack grows downward, so start at top */
    uint32_t *stack = (uint32_t*)(task->kernel_stack + 4096);
    
    /* Push initial values (will be popped by task_switch_asm) */
    *(--stack) = 0x202;                   /* EFLAGS (interrupts enabled) */
    *(--stack) = 0x08;                    /* CS (kernel code segment) */
    *(--stack) = (uint32_t)entry_point;   /* EIP */
    *(--stack) = 0;                       /* EAX */
    *(--stack) = 0;                       /* ECX */
    *(--stack) = 0;                       /* EDX */
    *(--stack) = 0;                       /* EBX */
    *(--stack) = 0;                       /* ESP (ignored) */
    *(--stack) = 0;                       /* EBP */
    *(--stack) = 0;                       /* ESI */
    *(--stack) = 0;                       /* EDI */
    
    /* Save stack pointer */
    task->context.esp = (uint32_t)stack;
    task->context.eip = (uint32_t)entry_point;
}

/* ================================================================
 * TASK SWITCHING (assembly stub will be created separately)
 * ================================================================ */

/* For now, simple C version (will be replaced with assembly) */
void task_switch(task_t *new_task)
{
    if (!new_task || new_task == current_task) {
        return;
    }
    
    task_t *old_task = current_task;
    
    /* Update states */
    if (old_task && old_task->state == TASK_RUNNING) {
        old_task->state = TASK_READY;
    }
    new_task->state = TASK_RUNNING;
    
    /* Update current */
    current_task = new_task;
    
    /* Actual context switch will be done in assembly */
    /* For now, this is a placeholder */
}

/* ================================================================
 * TASK QUERIES
 * ================================================================ */

task_t* task_current(void)
{
    return current_task;
}

/* ================================================================
 * TASK LIFECYCLE
 * ================================================================ */

void task_exit(int exit_code)
{
    if (!current_task || current_task == kernel_task) {
        return;
    }
    
    current_task->state = TASK_ZOMBIE;
    current_task->exit_code = exit_code;
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
    terminal_writestring("[TASK] Task '");
    terminal_writestring(current_task->name);
    terminal_writestring("' exited with code ");
    char buf[16];
    itoa(exit_code, buf);
    terminal_writestring(buf);
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    /* Yield to pick next task */
    task_yield();
    
    while(1) __asm__ volatile("hlt");
}

void task_yield(void)
{
    /* Will be hooked to scheduler */
    __asm__ volatile("hlt");
}

void task_block(void)
{
    if (current_task) {
        current_task->state = TASK_BLOCKED;
        task_yield();
    }
}

void task_unblock(task_t *task)
{
    if (task && task->state == TASK_BLOCKED) {
        task->state = TASK_READY;
    }
}

void task_sleep(uint32_t ms)
{
    (void)ms;
    task_block();
}

void task_destroy(task_t *task)
{
    if (!task || task == kernel_task) {
        return;
    }
    
    /* Remove from list */
    if (task_list_head == task) {
        task_list_head = task->next;
    } else {
        task_t *prev = task_list_head;
        while (prev && prev->next != task) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = task->next;
        }
    }
    
    /* Free resources */
    if (task->kernel_stack) kfree((void*)task->kernel_stack);
    if (task->user_stack) kfree((void*)task->user_stack);
    kfree(task);
}

/* ================================================================
 * IDLE TASK
 * ================================================================ */

static void idle_task_entry(void)
{
    while (1) {
        __asm__ volatile("hlt");
    }
}