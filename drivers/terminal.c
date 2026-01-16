/* drivers/terminal.c - VGA text mode display driver - WITH CURSOR SUPPORT
 *
 * CRITICAL FIX: Added hardware cursor updates so VGA display refreshes properly
 *
 * The VGA hardware has a cursor position that must be updated via I/O ports.
 * Without this, some display modes don't properly refresh the screen!
 */

#include "terminal.h"
#include "../kernel/kernel.h"
#include "../lib/string.h"

/* VGA text buffer - memory-mapped I/O at fixed address */
static uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;

/* VGA cursor control ports */
#define VGA_CTRL_REGISTER 0x3D4
#define VGA_DATA_REGISTER 0x3D5

/* Current cursor position - where next character will be written */
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;

/* Update hardware cursor position
 *
 * CRITICAL: This tells the VGA hardware where the cursor is.
 * Without this, some QEMU display modes don't refresh properly!
 *
 * The cursor position is a 16-bit value calculated as: row * 80 + column
 * We send it to the VGA controller in two 8-bit chunks.
 */
static void update_cursor(void)
{
    uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;

    /* Send high byte */
    outb(VGA_CTRL_REGISTER, 14); /* Cursor Location High Register */
    outb(VGA_DATA_REGISTER, (pos >> 8) & 0xFF);

    /* Send low byte */
    outb(VGA_CTRL_REGISTER, 15); /* Cursor Location Low Register */
    outb(VGA_DATA_REGISTER, pos & 0xFF);
}

/* Enable hardware cursor
 *
 * The VGA cursor can be enabled/disabled and styled.
 * This sets it to a visible underline cursor.
 */
static void enable_cursor(void)
{
    /* Set cursor style: underline from scanline 14 to 15 */
    outb(VGA_CTRL_REGISTER, 0x0A); /* Cursor Start Register */
    outb(VGA_DATA_REGISTER, 14);   /* Start at scanline 14 */

    outb(VGA_CTRL_REGISTER, 0x0B); /* Cursor End Register */
    outb(VGA_DATA_REGISTER, 15);   /* End at scanline 15 */
}

/* Helper: Combine foreground and background colors into VGA attribute byte */
uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg)
{
    return fg | (bg << 4);
}

/* Helper: Combine character and color into a VGA entry (16-bit value) */
uint16_t vga_entry(unsigned char c, uint8_t color)
{
    /* Ensure character is valid ASCII (0-127) to prevent weird characters */
    return (uint16_t)(c & 0x7F) | ((uint16_t)color << 8);
}

/* Scroll the terminal up by one line */
static void terminal_scroll(void)
{
    /* Copy each line to the previous line */
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            const size_t src = (y + 1) * VGA_WIDTH + x;
            const size_t dst = y * VGA_WIDTH + x;
            VGA_MEMORY[dst] = VGA_MEMORY[src];
        }
    }

    /* Clear the bottom line */
    const uint16_t blank = vga_entry(' ', terminal_color);
    for (size_t x = 0; x < VGA_WIDTH; x++)
    {
        const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        VGA_MEMORY[index] = blank;
    }

    /* Move cursor to start of bottom line */
    terminal_row = VGA_HEIGHT - 1;
    terminal_column = 0;
    update_cursor(); /* CRITICAL: Update hardware cursor */
}

/* Initialize terminal - clear screen and set up initial state */
void terminal_initialize(void)
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    /* Clear entire screen */
    const uint16_t blank = vga_entry(' ', terminal_color);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
    {
        VGA_MEMORY[i] = blank;
    }

    /* CRITICAL: Enable and position hardware cursor */
    enable_cursor();
    update_cursor();
}

/* Change current drawing color */
void terminal_setcolor(uint8_t color)
{
    terminal_color = color;
}

/* Clear the screen and reset cursor to top-left */
void terminal_clear(void)
{
    const uint16_t blank = vga_entry(' ', terminal_color);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
    {
        VGA_MEMORY[i] = blank;
    }
    terminal_row = 0;
    terminal_column = 0;
    update_cursor(); /* CRITICAL: Update hardware cursor */
}

/* Move to next line (like pressing Enter) */
void terminal_newline(void)
{
    terminal_column = 0;
    if (++terminal_row >= VGA_HEIGHT)
    {
        terminal_scroll();
    }
    else
    {
        update_cursor(); /* CRITICAL: Update hardware cursor */
    }
}

/* Write a single character to the screen
 *
 * CRITICAL FIX: Now updates hardware cursor after each character!
 * This ensures the VGA display refreshes properly.
 */
void terminal_putchar(char c)
{
    /* Handle special characters */
    switch (c)
    {
    case '\n': /* Newline */
        terminal_newline();
        return;

    case '\r': /* Carriage return */
        terminal_column = 0;
        update_cursor();
        return;

    case '\b': /* Backspace */
        if (terminal_column > 0)
        {
            terminal_column--;
            /* Replace with space to "erase" */
            const size_t index = terminal_row * VGA_WIDTH + terminal_column;
            VGA_MEMORY[index] = vga_entry(' ', terminal_color);
            update_cursor();
        }
        return;

    case '\t': /* Tab - align to next 4-column boundary */
        terminal_column = (terminal_column + 4) & ~(4 - 1);
        if (terminal_column >= VGA_WIDTH)
        {
            terminal_newline();
        }
        else
        {
            update_cursor();
        }
        return;
    }

    /* Ignore non-printable characters (except those handled above) */
    if (c < 32 || c > 126)
    {
        return; /* Skip control characters and extended ASCII */
    }

    /* Regular printable character - write to screen */
    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    VGA_MEMORY[index] = vga_entry(c, terminal_color);

    /* Advance cursor */
    if (++terminal_column >= VGA_WIDTH)
    {
        terminal_newline();
    }
    else
    {
        update_cursor(); /* CRITICAL: Update hardware cursor after each char */
    }
}

/* Write a null-terminated string to the screen */
void terminal_writestring(const char *data)
{
    if (!data)
        return; /* Safety check for null pointer */

    while (*data)
    {
        terminal_putchar(*data++);
    }
}

void terminal_write_dec(uint32_t n)
{
    char buffer[12]; // enough for 32-bit uint
    itoa(n, buffer);
    terminal_writestring(buffer);
}
