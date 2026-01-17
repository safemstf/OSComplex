/* terminal.h */

#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include "../kernel/kernel.h"   // brings enum vga_color into scope

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_writestring(const char* data);
void terminal_setcolor(uint8_t color);
void terminal_write_dec(uint32_t n);
void terminal_write_hex(uint32_t value);

/* Scrollback functions */
void terminal_scrollback_page_up(void);
void terminal_scrollback_page_down(void);

/* VGA helpers */
uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg);
uint16_t vga_entry(unsigned char c, uint8_t color);

#endif
