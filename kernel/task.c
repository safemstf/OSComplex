/* kernel/task.c - Task Management Implementation
 *
 * Implements process creation, destruction, and context switching.
 * UPDATED: Added process hierarchy management for fork/wait
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
#define USER_CODE_BASE 0x08048000 /* Standard Linux .text address */
/* USER_STACK_TOP is defined in vmm.h as 0xBFFFFFFF */
/* USER_STACK_SIZE is defined in vmm.h as 0x00100000 */
#define USER_HEAP_START 0x10000000 /* Heap starts at 256MB */

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
/* Add this idle loop function at the top of task.c, before task_init() */
static void kernel_idle_loop(void)
{
    terminal_writestring("[KERNEL_IDLE] Idle task running\n");
    while (1)
    {
        __asm__ volatile("hlt"); /* Halt until next interrupt */
    }
}

/* ================================================================
 * INITIALIZATION
 * ================================================================ */
void task_init(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[TASK] Initializing task management...\n");

    kernel_task = kmalloc(sizeof(task_t));
    if (!kernel_task)
    {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("[TASK] ERROR: Failed to allocate kernel task!\n");
        return;
    }

    memset(kernel_task, 0, sizeof(task_t));

    kernel_task->pid = 0;
    strcpy(kernel_task->name, "kernel_idle");
    kernel_task->state = TASK_RUNNING;
    kernel_task->priority = 255;
    kernel_task->ring = 0; /* Kernel mode */
    kernel_task->page_directory = vmm_current_as->page_dir;
    kernel_task->parent = NULL;
    kernel_task->parent_pid = 0;
    kernel_task->first_child = NULL;
    kernel_task->next_sibling = NULL;
    kernel_task->next = NULL;
    kernel_task->waited = false;

    /* Allocate kernel stack for idle task */
    uint32_t raw_kstack = (uint32_t)kmalloc(4096 + 4096);
    if (!raw_kstack)
    {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("[TASK] ERROR: Failed to allocate kernel stack!\n");
        kfree(kernel_task);
        return;
    }

    kernel_task->kernel_stack_alloc = raw_kstack;
    kernel_task->kernel_stack = (raw_kstack + 0xFFF) & ~0xFFF;

    /* Setup stack with idle loop as entry point */
    task_setup_kernel_stack(kernel_task, kernel_idle_loop);

    current_task = kernel_task;
    task_list_head = kernel_task;

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[TASK] Kernel idle task created (PID 0)\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

/* ================================================================
 * PROCESS HIERARCHY MANAGEMENT
 * ================================================================ */
void task_add_child(task_t *parent, task_t *child)
{
    if (!parent || !child)
        return;

    child->parent = parent;
    child->parent_pid = parent->pid;
    child->next_sibling = parent->first_child;
    parent->first_child = child;
}

void task_remove_child(task_t *parent, task_t *child)
{
    if (!parent || !child)
        return;

    task_t *prev = NULL;
    task_t *curr = parent->first_child;

    while (curr)
    {
        if (curr == child)
        {
            if (prev)
            {
                prev->next_sibling = curr->next_sibling;
            }
            else
            {
                parent->first_child = curr->next_sibling;
            }
            child->parent = NULL;
            child->next_sibling = NULL;
            return;
        }
        prev = curr;
        curr = curr->next_sibling;
    }
}

/* ================================================================
 * KERNEL TASK CREATION
 * ================================================================ */
task_t *task_create(const char *name, void (*entry_point)(void), uint32_t priority)
{
    task_t *task = kmalloc(sizeof(task_t));
    if (!task)
    {
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
    task->ring = 0; /* Kernel mode */
    task->time_slice = 10;
    task->total_time = 0;
    task->exit_code = 0;
    task->waited = false;

    /* Process hierarchy */
    task->parent = NULL;
    task->parent_pid = 0;
    task->first_child = NULL;
    task->next_sibling = NULL;

    /* Allocate kernel stack: produce a page-aligned stack and keep original pointer */
    uint32_t raw_kstack = (uint32_t)kmalloc(4096 + 4096);
    if (!raw_kstack)
    {
        terminal_writestring("[TASK] ERROR: Failed to allocate kernel stack\n");
        kfree(task);
        return NULL;
    }
    /* Align up to the next 4KB boundary */
    task->kernel_stack_alloc = raw_kstack; /* new field to store original pointer */
    task->kernel_stack = (raw_kstack + 0xFFF) & ~0xFFF;

    /* Allocate user stack */
    task->user_stack = (uint32_t)kmalloc(4096);
    if (!task->user_stack)
    {
        terminal_writestring("[TASK] ERROR: Failed to allocate user stack\n");
        if (task->kernel_stack_alloc)
            kfree((void *)task->kernel_stack_alloc);
        ;
        kfree(task);
        return NULL;
    }

    /* Use kernel address space */
    task->page_directory = kernel_task->page_directory;

    /* Setup initial stack */
    task_setup_kernel_stack(task, entry_point);

    /* Set parent to current task */
    if (current_task)
    {
        task_add_child(current_task, task);
    }

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
 * USER TASK CREATION (Phase 4) - FIXED VERSION
 * ================================================================ */
task_t *task_create_user(const char *name, void *elf_data, uint32_t priority)
{
    terminal_writestring("[TASK_CREATE_USER] Allocating task structure...\n");

    task_t *task = kmalloc(sizeof(task_t));
    if (!task)
        return NULL;

    memset(task, 0, sizeof(task_t));

    /* Basic properties */
    task->pid = next_pid++;
    strncpy(task->name, name, 31);
    task->name[31] = '\0';
    task->state = TASK_READY;
    task->priority = priority;
    task->ring = 3; /* USER MODE */
    task->time_slice = 10;
    task->first_run = true;

    /* ------------------------------------------------------------
     * Create a new address space
     * ------------------------------------------------------------ */
    struct vmm_address_space *as = vmm_create_as();
    if (!as)
    {
        kfree(task);
        return NULL;
    }

    task->address_space = as;
    task->page_directory = as->page_dir;

    terminal_writestring("[TASK_CREATE_USER] Page directory: 0x");
    terminal_write_hex((uint32_t)task->page_directory);
    terminal_writestring("\n");

    /* ------------------------------------------------------------
     * Allocate kernel stack (page-aligned)
     * ------------------------------------------------------------ */
    uint32_t raw_kstack = (uint32_t)kmalloc(4096 + 4096);
    if (!raw_kstack)
    {
        vmm_destroy_as(as);
        kfree(task);
        return NULL;
    }

    task->kernel_stack_alloc = raw_kstack;
    task->kernel_stack = (raw_kstack + 0xFFF) & ~0xFFF;

    terminal_writestring("[TASK_CREATE_USER] Kernel stack: 0x");
    terminal_write_hex(task->kernel_stack);
    terminal_writestring("\n");

    /* ------------------------------------------------------------
     * Allocate user stack (physical page)
     * ------------------------------------------------------------ */
    uint32_t ustack_phys = (uint32_t)pmm_alloc_block();
    if (!ustack_phys)
    {
        kfree((void *)raw_kstack);
        vmm_destroy_as(as);
        kfree(task);
        return NULL;
    }

    task->user_stack_phys = ustack_phys;

    terminal_writestring("[TASK_CREATE_USER] User stack phys: 0x");
    terminal_write_hex(ustack_phys);
    terminal_writestring("\n");

    /* ------------------------------------------------------------
     * Map user stack INTO TASK ADDRESS SPACE
     * ------------------------------------------------------------ */
    vmm_map_page_in_as(
        task->address_space,
        USER_STACK_TOP - PAGE_SIZE + 1,
        ustack_phys,
        VMM_PRESENT | VMM_WRITE | VMM_USER);

    task->user_esp = USER_STACK_TOP - 4;
    task->stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;

    terminal_writestring("[TASK_CREATE_USER] User ESP: 0x");
    terminal_write_hex(task->user_esp);
    terminal_writestring("\n");

    /* ------------------------------------------------------------
     * Load ELF
     * ------------------------------------------------------------ */
    if (!elf_load(task, elf_data))
    {
        vmm_unmap_page_in_as(task->address_space, USER_STACK_TOP - PAGE_SIZE);
        pmm_free_block((void *)ustack_phys);
        kfree((void *)raw_kstack);
        vmm_destroy_as(as);
        kfree(task);
        return NULL;
    }

    terminal_writestring("[TASK_CREATE_USER] ELF entry: 0x");
    terminal_write_hex(task->entry_point);
    terminal_writestring("\n");

    /* ------------------------------------------------------------
     * Build IRET frame
     * ------------------------------------------------------------ */
    task_setup_user_context(task);

    /* ------------------------------------------------------------
     * Parent / task list
     * ------------------------------------------------------------ */
    if (current_task)
        task_add_child(current_task, task);

    task->next = task_list_head;
    task_list_head = task;

    terminal_writestring("[TASK_CREATE_USER] ✓ User task created\n");
    return task;
}

/* ================================================================
 * STACK SETUP - KERNEL MODE
 * ================================================================ */
static void task_setup_kernel_stack(task_t *task, void (*entry_point)(void))
{
    uint32_t *stack = (uint32_t *)(task->kernel_stack + 4096);

    /* Push initial values (will be popped by task_switch_asm) */
    *(--stack) = 0x202;                 /* EFLAGS (interrupts enabled) */
    *(--stack) = 0x08;                  /* CS (kernel code segment) */
    *(--stack) = (uint32_t)entry_point; /* EIP */
    *(--stack) = 0;                     /* EAX */
    *(--stack) = 0;                     /* ECX */
    *(--stack) = 0;                     /* EDX */
    *(--stack) = 0;                     /* EBX */
    *(--stack) = 0;                     /* ESP (ignored) */
    *(--stack) = 0;                     /* EBP */
    *(--stack) = 0;                     /* ESI */
    *(--stack) = 0;                     /* EDI */

    task->context.esp = (uint32_t)stack;
    task->context.eip = (uint32_t)entry_point;
}

/* ================================================================
 * STACK SETUP - USER MODE
 * ================================================================ */
void task_setup_user_context(task_t *task)
{
    uint32_t *kstack = (uint32_t *)(task->kernel_stack + 4096);

    /*
     * iret frame for ring 3 transition
     * CPU pops in this order:
     *   EIP, CS, EFLAGS, ESP, SS
     */

    *(--kstack) = 0x23;              /* SS (user data) */
    *(--kstack) = task->user_esp;    /* ESP */
    *(--kstack) = 0x202;             /* EFLAGS (IF=1) */
    *(--kstack) = 0x1B;              /* CS (user code) */
    *(--kstack) = task->entry_point; /* EIP */

    task->context.esp = (uint32_t)kstack;
    task->context.eip = task->entry_point;
}

/* ================================================================
 * TASK SWITCHING
 * ================================================================ */
void task_switch(task_t *new_task)
{
    __asm__ volatile("cli");

    if (!new_task || new_task == current_task)
    {
        __asm__ volatile("sti"); // Re-enable if returning early
        return;
    }

    task_t *old_task = current_task;

    /* Update states */
    if (old_task && old_task->state == TASK_RUNNING)
        old_task->state = TASK_READY;

    new_task->state = TASK_RUNNING;
    current_task = new_task;

    /* Debug output */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK));
    terminal_writestring("[SWITCH] Switching to PID ");
    terminal_write_dec(new_task->pid);
    terminal_writestring(" (");
    terminal_writestring(new_task->name);
    terminal_writestring(")\n");
    terminal_writestring("[SWITCH] Ring=");
    terminal_write_dec(new_task->ring);
    terminal_writestring(", EIP=0x");
    terminal_write_hex(new_task->context.eip);
    terminal_writestring(", Entry=0x");
    terminal_write_hex(new_task->entry_point);
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    /* ADD DELAY so you can read it */
    for (volatile int i = 0; i < 10000000; i++)
        ;

    /* Switch page directory if different */
    if (new_task->page_directory &&
        (!old_task || new_task->page_directory != old_task->page_directory))
    {
        terminal_writestring("[SWITCH] Loading page directory 0x");
        terminal_write_hex((uint32_t)new_task->page_directory);
        terminal_writestring("\n");

        for (volatile int i = 0; i < 10000000; i++)
            ;

        uint32_t phys = (uint32_t)new_task->page_directory;
        __asm__ volatile("mov %0, %%cr3" ::"r"(phys));

        terminal_writestring("[SWITCH] Page directory loaded\n");
        for (volatile int i = 0; i < 10000000; i++)
            ;
    }

    /* Update TSS */
    extern void tss_set_kernel_stack(uint32_t stack);
    tss_set_kernel_stack(new_task->kernel_stack + 4096);

    /* USER MODE: Always IRET for ring 3 */
    if (new_task->ring == 3 && new_task->first_run)
    {
        terminal_writestring("[SWITCH] User mode - using IRET\n");
        terminal_writestring("[SWITCH] ESP=0x");
        terminal_write_hex(new_task->context.esp);
        terminal_writestring("\n");

        /* CRITICAL DEBUG: Show what's IN the IRET frame */
        uint32_t *iret_frame = (uint32_t *)new_task->context.esp;
        terminal_writestring("[IRET_FRAME] EIP=0x");
        terminal_write_hex(iret_frame[0]);
        terminal_writestring("\n");
        terminal_writestring("[IRET_FRAME] CS=0x");
        terminal_write_hex(iret_frame[1]);
        terminal_writestring("\n");
        terminal_writestring("[IRET_FRAME] EFLAGS=0x");
        terminal_write_hex(iret_frame[2]);
        terminal_writestring("\n");
        terminal_writestring("[IRET_FRAME] User ESP=0x");
        terminal_write_hex(iret_frame[3]);
        terminal_writestring("\n");
        terminal_writestring("[IRET_FRAME] SS=0x");
        terminal_write_hex(iret_frame[4]);
        terminal_writestring("\n");

        for (volatile int i = 0; i < 50000000; i++)
            ;

        /* ========== NEW DEBUG: Verify stack is mapped ========== */
        uint32_t stack_page = 0xBFFFF000; // The stack page
        uint32_t pd_idx = stack_page >> 22;
        uint32_t pt_idx = (stack_page >> 12) & 0x3FF;

        terminal_writestring("[DEBUG] Checking stack page 0xBFFFF000\n");
        terminal_writestring("[DEBUG] PD index: ");
        terminal_write_dec(pd_idx);
        terminal_writestring(", PT index: ");
        terminal_write_dec(pt_idx);
        terminal_writestring("\n");

        uint32_t *check_pd = new_task->page_directory;
        terminal_writestring("[DEBUG] PD entry for stack: 0x");
        terminal_write_hex(check_pd[pd_idx]);
        terminal_writestring("\n");

        if (check_pd[pd_idx] & VMM_PRESENT)
        {
            uint32_t *pt = (uint32_t *)(check_pd[pd_idx] & ~0xFFF);
            terminal_writestring("[DEBUG] PT entry for stack: 0x");
            terminal_write_hex(pt[pt_idx]);
            terminal_writestring("\n");
        }
        else
        {
            terminal_writestring("[ERROR] Stack page directory entry NOT PRESENT!\n");
        }

        /* Check current CS to verify we're in Ring 0 */
        uint16_t current_cs;
        __asm__ volatile("mov %%cs, %0" : "=r"(current_cs));
        terminal_writestring("[DEBUG] Current CS before IRET: 0x");
        terminal_write_hex(current_cs);
        terminal_writestring("\n");

        /* Also check if interrupts are disabled */
        uint32_t eflags;
        __asm__ volatile("pushf; pop %0" : "=r"(eflags));
        terminal_writestring("[DEBUG] EFLAGS: 0x");
        terminal_write_hex(eflags);
        terminal_writestring("\n");

        uint16_t current_ss;
        __asm__ volatile("mov %%ss, %0" : "=r"(current_ss));
        terminal_writestring("[DEBUG] Current SS before IRET: 0x");
        terminal_write_hex(current_ss);
        terminal_writestring("\n");

        for (volatile int i = 0; i < 50000000; i++)
            ;

        /* ========== DEBUG: Dump first bytes at entry point ========== */
        uint32_t entry = iret_frame[0];  /* EIP from IRET frame */
        terminal_writestring("[DEBUG] Verifying code at entry point 0x");
        terminal_write_hex(entry);
        terminal_writestring(":\n");

        /* Check code page mapping */
        uint32_t code_pd_idx = entry >> 22;
        uint32_t code_pt_idx = (entry >> 12) & 0x3FF;
        terminal_writestring("[DEBUG] Code PD index: ");
        terminal_write_dec(code_pd_idx);
        terminal_writestring(", PT index: ");
        terminal_write_dec(code_pt_idx);
        terminal_writestring("\n");

        terminal_writestring("[DEBUG] Code PD entry: 0x");
        terminal_write_hex(check_pd[code_pd_idx]);
        terminal_writestring("\n");

        if (check_pd[code_pd_idx] & VMM_PRESENT)
        {
            uint32_t *code_pt = (uint32_t *)(check_pd[code_pd_idx] & ~0xFFF);
            terminal_writestring("[DEBUG] Code PT entry: 0x");
            terminal_write_hex(code_pt[code_pt_idx]);
            terminal_writestring("\n");

            /* Get physical address and read from there via identity map */
            uint32_t code_phys = code_pt[code_pt_idx] & ~0xFFF;
            terminal_writestring("[DEBUG] Code physical addr: 0x");
            terminal_write_hex(code_phys);
            terminal_writestring("\n");

            /* Read from physical address (identity mapped) */
            uint8_t *phys_code_ptr = (uint8_t *)code_phys;
            terminal_writestring("[DEBUG] First 16 bytes (via phys): ");
            for (int i = 0; i < 16; i++) {
                terminal_write_hex(phys_code_ptr[i]);
                terminal_putchar(' ');
            }
            terminal_writestring("\n");
        }
        else
        {
            terminal_writestring("[ERROR] Code page directory entry NOT PRESENT!\n");
        }

        /* Also try reading through virtual address after CR3 switch */
        uint8_t *code_ptr = (uint8_t *)entry;
        terminal_writestring("[DEBUG] First 16 bytes (via virt): ");
        for (int i = 0; i < 16; i++) {
            terminal_write_hex(code_ptr[i]);
            terminal_putchar(' ');
        }
        terminal_writestring("\n");

        for (volatile int i = 0; i < 50000000; i++)
            ;

        new_task->first_run = false;
        uint32_t esp_val = new_task->context.esp;

        /* Zero all general purpose registers before IRET to avoid garbage */
        __asm__ volatile(
            "movl %[esp_val], %%esp\n\t"
            "xorl %%eax, %%eax\n\t"
            "xorl %%ebx, %%ebx\n\t"
            "xorl %%ecx, %%ecx\n\t"
            "xorl %%edx, %%edx\n\t"
            "xorl %%esi, %%esi\n\t"
            "xorl %%edi, %%edi\n\t"
            "xorl %%ebp, %%ebp\n\t"
            "iret\n\t"
            : /* no outputs */
            : [esp_val] "r"(esp_val)
            : "memory");

        // This should never execute if IRET works
        terminal_writestring("[ERROR] IRET returned to kernel mode!\n");
        while (1)
            __asm__ volatile("hlt");
    }
    /* Kernel mode */
    terminal_writestring("[SWITCH] Kernel mode - using task_switch_asm\n");
    for (volatile int i = 0; i < 10000000; i++)
        ;
    task_switch_asm(old_task, new_task);
}

/* ================================================================
 * TASK QUERIES
 * ================================================================ */
task_t *task_current(void)
{
    return current_task;
}

/* ================================================================
 * TASK LIFECYCLE
 * ================================================================ */
void task_exit(int exit_code)
{
    if (!current_task || current_task == kernel_task)
    {
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

    /* Wake up parent if it's waiting */
    if (current_task->parent && current_task->parent->state == TASK_BLOCKED)
    {
        task_unblock(current_task->parent);
    }

    task_yield();

    while (1)
        __asm__ volatile("hlt");
}

void task_yield(void)
{
    extern void scheduler_schedule(void);
    scheduler_schedule();
}

void task_block(void)
{
    if (current_task)
    {
        current_task->state = TASK_BLOCKED;
        task_yield();
    }
}

void task_unblock(task_t *task)
{
    if (task && task->state == TASK_BLOCKED)
    {
        task->state = TASK_READY;
    }
}

void task_sleep(uint32_t ms)
{
    if (!current_task)
        return;

    extern scheduler_stats_t scheduler_get_stats(void);
    scheduler_stats_t stats = scheduler_get_stats();

    current_task->wake_time = stats.total_ticks + ms;
    current_task->state = TASK_SLEEPING;

    task_yield();
}

void task_destroy(task_t *task)
{
    if (!task || task == kernel_task)
    {
        return;
    }

    /* Remove from parent's child list */
    if (task->parent)
    {
        task_remove_child(task->parent, task);
    }

    /* Remove from scheduler list */
    if (task_list_head == task)
    {
        task_list_head = task->next;
    }
    else
    {
        task_t *prev = task_list_head;
        while (prev && prev->next != task)
        {
            prev = prev->next;
        }
        if (prev)
        {
            prev->next = task->next;
        }
    }

    /* Free kernel stack */
    if (task->kernel_stack_alloc)
    {
        if (task->ring == 3)
        {
            /* User task: allocated with vmm_alloc_page */
            vmm_free_page((void *)task->kernel_stack_alloc);
        }
        else
        {
            /* Kernel task: allocated with kmalloc */
            kfree((void *)task->kernel_stack_alloc);
        }
    }

    /* Free user stack and address space for user tasks */
    if (task->ring == 3)
    {
        /* Unmap and free user stack */
        if (task->user_stack_phys)
        {
            vmm_unmap_page(USER_STACK_TOP - PAGE_SIZE);
            pmm_free_block((void *)task->user_stack_phys);
        }

        /* Destroy address space */
        if (task->address_space)
        {
            vmm_destroy_as(task->address_space);
        }
    }
    else
    {
        /* Kernel task: free user stack if allocated */
        if (task->user_stack)
        {
            kfree((void *)task->user_stack);
        }
    }

    /* Free task structure */
    kfree(task);
}