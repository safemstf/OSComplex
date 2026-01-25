/* kernel/scheduler.c - Round-Robin Task Scheduler
 * 
 * Manages task execution order using round-robin algorithm.
 * Each task gets an equal time slice (10ms by default).
 */

#include "scheduler.h"
#include "kernel.h"
#include "task.h"

/* ================================================================
 * GLOBAL STATE
 * ================================================================ */

static scheduler_stats_t stats = {0};
static bool scheduler_running = false;

/* Ready queue - circular linked list of tasks */
static task_t *ready_queue = NULL;
static task_t *ready_queue_tail = NULL;

/* External references */
extern task_t *current_task;
extern task_t *kernel_task;

/* Forward declarations */
static void update_statistics(void);
static task_t* get_next_ready_task(void);

/* ================================================================
 * INITIALIZATION
 * ================================================================ */

void scheduler_init(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[SCHEDULER] Initializing scheduler...\n");
    
    memset(&stats, 0, sizeof(stats));
    
    ready_queue = NULL;
    ready_queue_tail = NULL;
    scheduler_running = false;
    
    /* Add kernel task to ready queue */
    if (kernel_task) {
        scheduler_add_task(kernel_task);
    }
    
    scheduler_running = true;
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[SCHEDULER] Scheduler ready\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

/* ================================================================
 * TASK QUEUE MANAGEMENT
 * ================================================================ */

void scheduler_add_task(task_t *task)
{
    if (!task) return;
    
    /* Don't add if already in queue */
    task_t *curr = ready_queue;
    while (curr) {
        if (curr == task) return;
        if (curr->next == ready_queue) break;  /* Full circle */
        curr = curr->next;
    }
    
    /* Add to ready queue */
    if (!ready_queue) {
        /* First task in queue */
        ready_queue = task;
        ready_queue_tail = task;
        task->next = task;  /* Circular */
    } else {
        /* Add to end */
        ready_queue_tail->next = task;
        ready_queue_tail = task;
        task->next = ready_queue;  /* Circular */
    }
    
    task->state = TASK_READY;
    stats.total_tasks++;
}

void scheduler_remove_task(task_t *task)
{
    if (!task || !ready_queue) return;
    
    /* Find and remove from queue */
    if (ready_queue == task) {
        /* Removing head */
        if (ready_queue == ready_queue_tail) {
            /* Only task in queue */
            ready_queue = NULL;
            ready_queue_tail = NULL;
        } else {
            ready_queue_tail->next = task->next;
            ready_queue = task->next;
        }
    } else {
        /* Find previous task */
        task_t *prev = ready_queue;
        while (prev->next != task && prev->next != ready_queue) {
            prev = prev->next;
        }
        
        if (prev->next == task) {
            prev->next = task->next;
            if (task == ready_queue_tail) {
                ready_queue_tail = prev;
            }
        }
    }
    
    task->next = NULL;
    if (stats.total_tasks > 0) stats.total_tasks--;
}

/* ================================================================
 * TASK SELECTION
 * ================================================================ */

static task_t* get_next_ready_task(void)
{
    if (!ready_queue) return kernel_task;
    
    /* Start from current position in queue */
    task_t *start = current_task ? current_task->next : ready_queue;
    task_t *task = start;
    
    /* Find next READY task */
    do {
        if (task->state == TASK_READY) {
            return task;
        }
        task = task->next;
    } while (task != start);
    
    /* No ready tasks, return kernel idle */
    return kernel_task;
}

task_t* scheduler_pick_next(void)
{
    update_statistics();
    
    /* Get next ready task */
    task_t *next = get_next_ready_task();
    
    /* Reset time slice */
    next->time_slice = SCHEDULER_TIME_SLICE_MS;
    
    return next;
}

/* ================================================================
 * SCHEDULING
 * ================================================================ */

void scheduler_tick(void)
{
    if (!scheduler_running) return;
    
    stats.total_ticks++;
    
    /* Update sleeping tasks */
    task_t *task = ready_queue;
    if (task) {
        task_t *start = task;
        do {
            if (task->state == TASK_SLEEPING) {
                if (task->wake_time > 0 && stats.total_ticks >= task->wake_time) {
                    task->state = TASK_READY;
                    task->wake_time = 0;
                }
            }
            task = task->next;
        } while (task != start);
    }
    
    /* Decrement current task's time slice */
    if (current_task && current_task->state == TASK_RUNNING) {
        if (current_task->time_slice > 0) {
            current_task->time_slice--;
            current_task->total_time++;
        }
        
        /* Time slice expired? */
        if (current_task->time_slice == 0) {
            scheduler_schedule();
        }
    }
}

void scheduler_schedule(void)
{
    if (!scheduler_running) return;

    /* Pick next task */
    task_t *next = scheduler_pick_next();

    if (!next || next == current_task) {
        /* Reset current task's time slice */
        if (current_task) {
            current_task->time_slice = SCHEDULER_TIME_SLICE_MS;
        }
        return;
    }

    stats.context_switches++;

    /* Context switch happens here */
    task_switch(next);
}

/* ================================================================
 * STATISTICS
 * ================================================================ */

static void update_statistics(void)
{
    stats.ready_tasks = 0;
    stats.blocked_tasks = 0;
    
    task_t *task = ready_queue;
    if (!task) return;
    
    task_t *start = task;
    do {
        switch (task->state) {
            case TASK_READY:
                stats.ready_tasks++;
                break;
            case TASK_BLOCKED:
            case TASK_SLEEPING:
                stats.blocked_tasks++;
                break;
            default:
                break;
        }
        task = task->next;
    } while (task != start);
}

scheduler_stats_t scheduler_get_stats(void)
{
    update_statistics();
    return stats;
}
