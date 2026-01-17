/* drivers/timer.h - PIT Timer Interface */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Initialize the timer */
void timer_init(void);

/* Get number of ticks since boot */
uint32_t timer_get_ticks(void);

/* Sleep for ms milliseconds */
void timer_sleep(uint32_t ms);

/* Timer interrupt handler (called by IRQ 0) */
void timer_handler(void);

#endif /* TIMER_H */