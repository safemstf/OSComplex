/* kernel/scheduler.h - Task Scheduler
 * 
 * The scheduler decides which task runs when.
 * We use a simple round-robin algorithm with priorities.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"

/* ================================================================
 * SCHEDULER CONFIGURATION
 * ================================================================ */

#define SCHEDULER_TIME_SLICE_MS 10    /* Each task gets 10ms */
#define SCHEDULER_MAX_TASKS 64        /* Maximum concurrent tasks */

/* ================================================================
 * SCHEDULER FUNCTIONS
 * ================================================================ */

/* Initialize the scheduler */
void scheduler_init(void);

/* Add a task to the scheduler */
void scheduler_add_task(task_t *task);

/* Remove a task from the scheduler */
void scheduler_remove_task(task_t *task);

/* Pick the next task to run (called by timer interrupt) */
task_t* scheduler_pick_next(void);

/* Perform a context switch (called by timer interrupt) */
void scheduler_schedule(void);

/* Timer tick - called every millisecond */
void scheduler_tick(void);

/* Get scheduler statistics */
typedef struct {
    uint32_t total_tasks;
    uint32_t ready_tasks;
    uint32_t blocked_tasks;
    uint32_t context_switches;
    uint32_t total_ticks;
} scheduler_stats_t;

scheduler_stats_t scheduler_get_stats(void);

#endif /* SCHEDULER_H */