/* kernel/scheduler.c - Task Scheduler Implementation
 * 
 * Implements a simple round-robin scheduler with priorities.
 * Called by timer interrupt every 10ms to switch tasks.
 */

#include "scheduler.h"
#include "task.h"
#include "kernel.h"

/* ================================================================
 * SCHEDULER STATE
 * ================================================================ */

static scheduler_stats_t stats = {0};
static bool scheduler_enabled = false;

/* ================================================================
 * INITIALIZATION
 * ================================================================ */

void scheduler_init(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[SCHEDULER] Initializing scheduler...\n");
    
    memset(&stats, 0, sizeof(stats));
    scheduler_enabled = true;
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[SCHEDULER] Round-robin scheduler ready (");
    char buf[16];
    itoa(SCHEDULER_TIME_SLICE_MS, buf);
    terminal_writestring(buf);
    terminal_writestring("ms time slice)\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

/* ================================================================
 * TASK QUEUE MANAGEMENT
 * ================================================================ */

void scheduler_add_task(task_t *task)
{
    if (!task) return;
    
    stats.total_tasks++;
    if (task->state == TASK_READY) {
        stats.ready_tasks++;
    } else if (task->state == TASK_BLOCKED) {
        stats.blocked_tasks++;
    }
}

void scheduler_remove_task(task_t *task)
{
    if (!task) return;
    
    if (stats.total_tasks > 0) {
        stats.total_tasks--;
    }
    
    if (task->state == TASK_READY && stats.ready_tasks > 0) {
        stats.ready_tasks--;
    } else if (task->state == TASK_BLOCKED && stats.blocked_tasks > 0) {
        stats.blocked_tasks--;
    }
}

/* ================================================================
 * SCHEDULER ALGORITHM
 * ================================================================ */

task_t* scheduler_pick_next(void)
{
    task_t *current = task_current();
    if (!current) {
        return kernel_task;
    }
    
    /* Simple round-robin: find next READY task */
    task_t *next = current->next;
    
    if (!next) {
        next = kernel_task;
    }
    
    /* Find first ready task */
    task_t *start = next;
    while (next) {
        if (next->state == TASK_READY || next->state == TASK_RUNNING) {
            return next;
        }
        
        next = next->next;
        if (!next) {
            next = kernel_task;
        }
        
        if (next == start) {
            break;
        }
    }
    
    return kernel_task;
}

void scheduler_schedule(void)
{
    if (!scheduler_enabled) {
        return;
    }
    
    task_t *current = task_current();
    if (!current) {
        return;
    }
    
    /* Decrement time slice */
    if (current->time_slice > 0) {
        current->time_slice--;
    }
    
    /* If time slice expired, pick next task */
    if (current->time_slice == 0) {
        task_t *next = scheduler_pick_next();
        
        if (next && next != current) {
            next->time_slice = SCHEDULER_TIME_SLICE_MS;
            stats.context_switches++;
            task_switch(next);
        } else {
            current->time_slice = SCHEDULER_TIME_SLICE_MS;
        }
    }
}

/* ================================================================
 * TIMER INTEGRATION
 * ================================================================ */

void scheduler_tick(void)
{
    stats.total_ticks++;
    
    task_t *current = task_current();
    if (current) {
        current->total_time++;
    }
    
    scheduler_schedule();
}

/* ================================================================
 * STATISTICS
 * ================================================================ */

scheduler_stats_t scheduler_get_stats(void)
{
    return stats;
}