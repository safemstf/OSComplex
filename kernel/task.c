/* kernel/task.c - Task Management Implementation
 * 
 * Implements process creation, destruction, and context switching.
 * CLEANED: Removed duplicate code, organized clearly
 * FIXED: Removed macro conflicts and static declaration issues
 */

#include "task.h"
#include "kernel.h"
#include "elf.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "scheduler.h"

/* ================================================================
 * USER MODE MEMORY LAYOUT
 * ================================================================ */
#define USER_CODE_BASE   0x08048000  /* Standard Linux .text address */
/* USER_STACK_TOP is defined in vmm.h as 0xBFFFFFFF */
/* USER_STACK_SIZE is defined in vmm.h as 0x00100000 */
#define USER_HEAP_START  0x10000000  /* Heap starts at 256MB */

/* ================================================================
 * GLOBAL STATE
 * ================================================================ */
task_t *current_task = NULL;
static uint32_t next_pid = 1;
task_t *kernel_task = NULL;
static task_t *task_list_head = NULL;

/* ================================================================
 * FORWARD DECLARATIONS
 * ================================================================ */
static void task_setup_kernel_stack(task_t *task, void (*entry_point)(void));
extern void task_switch_asm(task_t *old_task, task_t *new_task);

/* ================================================================
 * INITIALIZATION
 * ================================================================ */
void task_init(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[TASK] Initializing task management...\n");
    
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
    kernel_task->priority = 255;
    kernel_task->ring = 0;  /* Kernel mode */
    kernel_task->page_directory = vmm_current_as->page_dir;
    kernel_task->parent = NULL;
    kernel_task->next = NULL;
    
    current_task = kernel_task;
    task_list_head = kernel_task;
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[TASK] Kernel idle task created (PID 0)\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

/* ================================================================
 * KERNEL TASK CREATION
 * ================================================================ */
task_t* task_create(const char *name, void (*entry_point)(void), uint32_t priority)
{
    task_t *task = kmalloc(sizeof(task_t));
    if (!task) {
        terminal_writestring("[TASK] ERROR: Failed to allocate task structure\n");
        return NULL;
    }
    
    memset(task, 0, sizeof(task_t));
    
    /* Basic properties */
    task->pid = next_pid++;
    strncpy(task->name, name, 31);
    task->name[31] = '\0';
    task->state = TASK_READY;
    task->priority = priority;
    task->ring = 0;  /* Kernel mode */
    task->time_slice = 10;
    task->total_time = 0;
    task->exit_code = 0;
    
    /* Allocate kernel stack */
    task->kernel_stack = (uint32_t)kmalloc(4096);
    if (!task->kernel_stack) {
        terminal_writestring("[TASK] ERROR: Failed to allocate kernel stack\n");
        kfree(task);
        return NULL;
    }
    
    /* Allocate user stack */
    task->user_stack = (uint32_t)kmalloc(4096);
    if (!task->user_stack) {
        terminal_writestring("[TASK] ERROR: Failed to allocate user stack\n");
        kfree((void*)task->kernel_stack);
        kfree(task);
        return NULL;
    }
    
    /* Use kernel address space */
    task->page_directory = kernel_task->page_directory;
    
    /* Setup initial stack */
    task_setup_kernel_stack(task, entry_point);
    
    /* Set parent */
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
 * USER TASK CREATION (Phase 4)
 * ================================================================ */
task_t* task_create_user(const char *name, void *elf_data, uint32_t priority)
{
    terminal_writestring("[TASK_CREATE_USER] Step 1: Allocating task structure...\n");
    
    task_t *task = kmalloc(sizeof(task_t));
    if (!task) {
        terminal_writestring("[TASK_CREATE_USER] ERROR: Failed to allocate task\n");
        return NULL;
    }
    
    terminal_writestring("[TASK_CREATE_USER] Step 2: Initializing task structure...\n");
    memset(task, 0, sizeof(task_t));
    
    /* Basic properties */
    task->pid = next_pid++;
    strncpy(task->name, name, 31);
    task->name[31] = '\0';
    task->state = TASK_READY;
    task->priority = priority;
    task->ring = 3;  /* USER MODE! */
    task->time_slice = 10;
    
    terminal_writestring("[TASK_CREATE_USER] Step 3: PID=");
    terminal_write_dec(task->pid);
    terminal_writestring(", Name=");
    terminal_writestring(task->name);
    terminal_writestring("\n");
    
    terminal_writestring("[TASK_CREATE_USER] Step 4: Creating new page directory...\n");
    struct vmm_address_space *as = vmm_create_as();
    if (!as) {
        terminal_writestring("[TASK_CREATE_USER] ERROR: vmm_create_as failed\n");
        kfree(task);
        return NULL;
    }
    task->page_directory = as->page_dir;
    
    terminal_writestring("[TASK_CREATE_USER] Step 5: Page directory at 0x");
    terminal_write_hex((uint32_t)task->page_directory);
    terminal_writestring("\n");
    
    terminal_writestring("[TASK_CREATE_USER] Step 6: Allocating kernel stack...\n");
    task->kernel_stack = (uint32_t)kmalloc(4096);
    if (!task->kernel_stack) {
        terminal_writestring("[TASK_CREATE_USER] ERROR: Failed to allocate kernel stack\n");
        vmm_destroy_as(as);
        kfree(task);
        return NULL;
    }
    
    terminal_writestring("[TASK_CREATE_USER] Step 7: Kernel stack at 0x");
    terminal_write_hex(task->kernel_stack);
    terminal_writestring("\n");
    
    terminal_writestring("[TASK_CREATE_USER] Step 8: Allocating user stack...\n");
    uint32_t stack_phys = (uint32_t)pmm_alloc_block();
    if (!stack_phys) {
        terminal_writestring("[TASK_CREATE_USER] ERROR: Failed to allocate user stack\n");
        kfree((void*)task->kernel_stack);
        vmm_destroy_as(as);
        kfree(task);
        return NULL;
    }
    
    terminal_writestring("[TASK_CREATE_USER] Step 9: User stack physical page at 0x");
    terminal_write_hex(stack_phys);
    terminal_writestring("\n");
    
    terminal_writestring("[TASK_CREATE_USER] Step 10: Mapping user stack...\n");
    vmm_map_page(USER_STACK_TOP - PAGE_SIZE, stack_phys, 
                 VMM_PRESENT | VMM_WRITE | VMM_USER);
    
    task->user_esp = USER_STACK_TOP;
    task->stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;
    
    terminal_writestring("[TASK_CREATE_USER] Step 11: User ESP=0x");
    terminal_write_hex(task->user_esp);
    terminal_writestring("\n");
    
    terminal_writestring("[TASK_CREATE_USER] Step 12: Loading ELF binary...\n");
    if (!elf_load(task, elf_data)) {
        terminal_writestring("[TASK_CREATE_USER] ERROR: elf_load failed\n");
        vmm_unmap_page(USER_STACK_TOP - PAGE_SIZE);
        pmm_free_block((void*)stack_phys);
        kfree((void*)task->kernel_stack);
        vmm_destroy_as(as);
        kfree(task);
        return NULL;
    }
    
    terminal_writestring("[TASK_CREATE_USER] Step 13: ELF loaded, entry point=0x");
    terminal_write_hex(task->code_start);
    terminal_writestring("\n");
    
    terminal_writestring("[TASK_CREATE_USER] Step 14: Setting up user context...\n");
    task_setup_user_context(task);
    
    terminal_writestring("[TASK_CREATE_USER] Step 15: Context setup complete\n");
    terminal_writestring("[TASK_CREATE_USER]   ESP=0x");
    terminal_write_hex(task->context.esp);
    terminal_writestring(", EIP=0x");
    terminal_write_hex(task->context.eip);
    terminal_writestring("\n");
    
    terminal_writestring("[TASK_CREATE_USER] Step 16: Setting parent task...\n");
    task->parent = current_task;
    
    terminal_writestring("[TASK_CREATE_USER] Step 17: Adding to task list...\n");
    terminal_writestring("[TASK_CREATE_USER]   task_list_head before: 0x");
    terminal_write_hex((uint32_t)task_list_head);
    terminal_writestring("\n");
    
    task->next = task_list_head;
    task_list_head = task;
    
    terminal_writestring("[TASK_CREATE_USER]   task_list_head after: 0x");
    terminal_write_hex((uint32_t)task_list_head);
    terminal_writestring("\n");
    terminal_writestring("[TASK_CREATE_USER]   task address: 0x");
    terminal_write_hex((uint32_t)task);
    terminal_writestring("\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[TASK_CREATE_USER] ✓ User task created successfully!\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    return task;
}

/* ================================================================
 * STACK SETUP - KERNEL MODE
 * ================================================================ */
static void task_setup_kernel_stack(task_t *task, void (*entry_point)(void))
{
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
    
    task->context.esp = (uint32_t)stack;
    task->context.eip = (uint32_t)entry_point;
}

/* ================================================================
 * STACK SETUP - USER MODE
 * ================================================================ */
void task_setup_user_context(task_t *task)
{
    uint32_t *kstack = (uint32_t*)(task->kernel_stack + 4096);
    
    /* Build iret frame on kernel stack for returning to user mode */
    *(--kstack) = 0x23;                    /* SS (user data segment) */
    *(--kstack) = task->user_esp;          /* ESP (user stack) */
    *(--kstack) = 0x202;                   /* EFLAGS (IF=1) */
    *(--kstack) = 0x1B;                    /* CS (user code segment) */
    *(--kstack) = task->code_start;        /* EIP (entry point) */
    
    /* General purpose registers (will be popped by popa) */
    *(--kstack) = 0;  /* EAX */
    *(--kstack) = 0;  /* ECX */
    *(--kstack) = 0;  /* EDX */
    *(--kstack) = 0;  /* EBX */
    *(--kstack) = 0;  /* ESP (ignored) */
    *(--kstack) = 0;  /* EBP */
    *(--kstack) = 0;  /* ESI */
    *(--kstack) = 0;  /* EDI */
    
    /* Segment registers */
    *(--kstack) = 0x23;  /* DS */
    *(--kstack) = 0x23;  /* ES */
    *(--kstack) = 0x23;  /* FS */
    *(--kstack) = 0x23;  /* GS */
    
    task->context.esp = (uint32_t)kstack;
}

/* ================================================================
 * TASK SWITCHING
 * ================================================================ */
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
    
    /* Call assembly context switch */
    task_switch_asm(old_task, new_task);
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
    
    task_yield();
    
    while(1) __asm__ volatile("hlt");
}

void task_yield(void)
{
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
    if (!current_task) return;
    
    extern scheduler_stats_t scheduler_get_stats(void);
    scheduler_stats_t stats = scheduler_get_stats();
    
    current_task->wake_time = stats.total_ticks + ms;
    current_task->state = TASK_SLEEPING;
    
    task_yield();
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