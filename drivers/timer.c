/* drivers/timer.c - PIT (Programmable Interval Timer) driver
 * 
 * Drives the scheduler by calling scheduler_tick() every millisecond
 */

#include "../kernel/kernel.h"
#include "../kernel/scheduler.h"

#define PIT_FREQUENCY 1193182  /* PIT oscillator frequency in Hz */
#define TIMER_HZ 1000          /* We want 1000 ticks per second (1ms) */

static volatile uint32_t timer_ticks = 0;

/* Timer interrupt handler - called by IRQ 0 */
void timer_handler(void)
{
    timer_ticks++;
    
    /* Call scheduler every tick */
    scheduler_tick();
    
    /* Send EOI to PIC */
    pic_send_eoi(IRQ_TIMER);
}

/* Initialize PIT to generate interrupts at TIMER_HZ */
void timer_init(void)
{
    /* Calculate divisor for desired frequency */
    uint32_t divisor = PIT_FREQUENCY / TIMER_HZ;
    
    /* Send command byte: channel 0, lobyte/hibyte, rate generator */
    outb(0x43, 0x36);
    
    /* Send divisor */
    outb(0x40, (uint8_t)(divisor & 0xFF));        /* Low byte */
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF)); /* High byte */
    
    /* Install handler for IRQ 0 */
    irq_install_handler(IRQ_TIMER, timer_handler);
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[TIMER] PIT initialized (");
    char buf[16];
    itoa(TIMER_HZ, buf);
    terminal_writestring(buf);
    terminal_writestring(" Hz)\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

/* Get number of timer ticks since boot */
uint32_t timer_get_ticks(void)
{
    return timer_ticks;
}

/* Sleep for approximately ms milliseconds */
void timer_sleep(uint32_t ms)
{
    uint32_t target = timer_ticks + ms;
    while (timer_ticks < target) {
        __asm__ volatile("hlt");
    }
}