/* drivers/terminal.c - VGA text mode display driver
 *
 * Preserves hardware cursor support and adds a scrollback buffer,
 * page-up / page-down rendering, and automatic follow/unfollow behavior.
 *
 * Key points:
 * - VGA_MEMORY is volatile (MMIO).
 * - Scrollback stores full VGA entries (char+attr) in RAM.
 * - When viewport_top == 0, console "follows" live output.
 * - When viewport_top > 0, console shows older lines (user scrolled up).
 * - Use terminal_scrollback_page_up()/terminal_scrollback_page_down()
 *   from your keyboard handler (PgUp/PgDn).
 */

#include "terminal.h"
#include "../kernel/kernel.h"
#include "../lib/string.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* VGA text buffer - memory-mapped I/O at fixed address (must be volatile) */
static volatile uint16_t *const VGA_MEMORY = (volatile uint16_t *)0xB8000;

/* VGA cursor control ports */
#define VGA_CTRL_REGISTER 0x3D4
#define VGA_DATA_REGISTER 0x3D5

/* Scrollback configuration */
#define SCROLLBACK_LINES 1024  /* adjust if you need more/less RAM */

/* Current drawing/color state */
static uint8_t terminal_color;

/* Scrollback storage: store full VGA entries (char + attr) */
static uint16_t scrollback[SCROLLBACK_LINES][VGA_WIDTH];
static size_t scrollback_count = 0; /* number of lines currently stored (0..SCROLLBACK_LINES) */

/* Where the next character will be written in the logical buffer */
static size_t logical_row;   /* line index within the logical stream (0..scrollback_count-1) */
static size_t logical_col;   /* column within line (0..VGA_WIDTH-1) */

/* Viewport controls */
static size_t viewport_top = 0; /* 0 == follow latest; >0 == scrolled up N lines */
static bool scroll_locked = false; /* true when viewport_top > 0 */

/* Hardware-visible cursor row/col (these are the ones update_cursor uses) */
static size_t hw_cursor_row;
static size_t hw_cursor_col;

/* ======================================================================
 * low-level VGA helpers (unchanged)
 * ===================================================================== */

static void update_cursor(void)
{
    uint16_t pos = (uint16_t)(hw_cursor_row * VGA_WIDTH + hw_cursor_col);

    /* Send high byte */
    outb(VGA_CTRL_REGISTER, 14); /* Cursor Location High Register */
    outb(VGA_DATA_REGISTER, (pos >> 8) & 0xFF);

    /* Send low byte */
    outb(VGA_CTRL_REGISTER, 15); /* Cursor Location Low Register */
    outb(VGA_DATA_REGISTER, pos & 0xFF);
}

static void enable_cursor(void)
{
    /* Set cursor style: underline from scanline 14 to 15 */
    outb(VGA_CTRL_REGISTER, 0x0A); /* Cursor Start Register */
    outb(VGA_DATA_REGISTER, 14);   /* Start at scanline 14 */

    outb(VGA_CTRL_REGISTER, 0x0B); /* Cursor End Register */
    outb(VGA_DATA_REGISTER, 15);   /* End at scanline 15 */
}

uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg)
{
    return fg | (bg << 4);
}

uint16_t vga_entry(unsigned char c, uint8_t color)
{
    return (uint16_t)(c & 0x7F) | ((uint16_t)color << 8);
}

/* ======================================================================
 * scrollback / rendering helpers
 * ===================================================================== */

/* Helper: copy pointer to scrollback row */
static inline uint16_t *scallback_row_ptr_at(size_t idx) {
    return scrollback[idx];
}

/* Render the viewport (copy SCROLLBACK -> VGA). If viewport_top == 0, shows the latest
 * VGA_HEIGHT lines; otherwise shows older lines. */
static void terminal_render_viewport(void)
{
    /* Determine start line in scrollback to render to top of screen */
    size_t start;
    if (scrollback_count == 0) {
        /* nothing in scrollback yet -> clear VGA */
        uint16_t blank = vga_entry(' ', terminal_color);
        for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
            VGA_MEMORY[i] = blank;
        hw_cursor_row = 0;
        hw_cursor_col = 0;
        update_cursor();
        return;
    }

    if (scrollback_count > VGA_HEIGHT + viewport_top) {
        start = scrollback_count - VGA_HEIGHT - viewport_top;
    } else {
        start = 0;
    }

    /* Copy lines from scrollback to VGA */
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        size_t line_idx = start + y;
        if (line_idx < scrollback_count) {
            /* copy whole row (VGA_WIDTH entries) */
            memcpy((void*)&VGA_MEMORY[y * VGA_WIDTH],
                   (const void*)scallback_row_ptr_at(line_idx),
                   VGA_WIDTH * sizeof(uint16_t));
        } else {
            /* blank the rest */
            uint16_t blank = vga_entry(' ', terminal_color);
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                VGA_MEMORY[y * VGA_WIDTH + x] = blank;
            }
        }
    }

    /* Place hardware cursor if the logical cursor is visible within viewport */
    if (logical_row >= start && logical_row < start + VGA_HEIGHT) {
        hw_cursor_row = logical_row - start;
        hw_cursor_col = logical_col;
    } else {
        /* cursor off-screen: place it at bottom-left so update_cursor keeps things sane */
        hw_cursor_row = VGA_HEIGHT - 1;
        hw_cursor_col = 0;
    }
    update_cursor();
}

/* Append a new blank line to scrollback (manages capacity by shifting if full) */
static void scrollback_append_blank_line(void)
{
    if (scrollback_count < SCROLLBACK_LINES) {
        /* initialize a new blank line at the end */
        uint16_t blank = vga_entry(' ', terminal_color);
        for (size_t x = 0; x < VGA_WIDTH; x++)
            scrollback[scrollback_count][x] = blank;
        scrollback_count++;
    } else {
        /* buffer full: drop oldest line by shifting everything up 1 */
        memmove(scrollback,
                scrollback + 1,
                (SCROLLBACK_LINES - 1) * VGA_WIDTH * sizeof(uint16_t));
        /* clear last line */
        uint16_t blank = vga_entry(' ', terminal_color);
        for (size_t x = 0; x < VGA_WIDTH; x++)
            scrollback[SCROLLBACK_LINES - 1][x] = blank;
        /* scrollback_count remains SCROLLBACK_LINES */
    }
}

/* Ensure there's at least one line in the scrollback (called at init) */
static void ensure_scrollback_started(void)
{
    if (scrollback_count == 0) {
        scrollback_append_blank_line();
        logical_row = 0;
        logical_col = 0;
    }
}

/* Convenience: make the viewport follow the latest output again */
static void terminal_resume_follow(void)
{
    viewport_top = 0;
    scroll_locked = false;
    terminal_render_viewport();
}

/* Called when we add a new character or newline and the user isn't scrolled up:
 * fast-path to update only the affected VGA row instead of full render */
static void terminal_update_vga_row(size_t row_in_vga, size_t logical_row_idx)
{
    memcpy((void*)&VGA_MEMORY[row_in_vga * VGA_WIDTH],
           (const void*)scallback_row_ptr_at(logical_row_idx),
           VGA_WIDTH * sizeof(uint16_t));
}

/* ======================================================================
 * high-level terminal operations (print, scrollback controls)
 * ===================================================================== */

/* Public: page up (move viewport older) */
void terminal_scrollback_page_up(void)
{
    if (scrollback_count <= VGA_HEIGHT)
        return; /* nothing to scroll */

    /* Increase viewport_top by one page, but cap it so we don't go past earliest line */
    size_t max_top = (scrollback_count > VGA_HEIGHT) ? (scrollback_count - VGA_HEIGHT) : 0;
    if (viewport_top + VGA_HEIGHT <= max_top)
        viewport_top += VGA_HEIGHT;
    else
        viewport_top = max_top;

    scroll_locked = (viewport_top > 0);
    terminal_render_viewport();
}

/* Public: page down (move viewport toward newest). When viewport_top reaches 0, resume follow. */
void terminal_scrollback_page_down(void)
{
    if (viewport_top == 0)
        return;

    if (viewport_top > VGA_HEIGHT)
        viewport_top -= VGA_HEIGHT;
    else
        viewport_top = 0;

    scroll_locked = (viewport_top > 0);
    if (!scroll_locked)
        terminal_resume_follow();
    else
        terminal_render_viewport();
}

/* Initialize terminal - clear screen and set up initial state */
void terminal_initialize(void)
{
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    /* Clear VGA */
    const uint16_t blank = vga_entry(' ', terminal_color);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = blank;
    }

    /* Initialize scrollback */
    scrollback_count = 0;
    logical_row = 0;
    logical_col = 0;
    viewport_top = 0;
    scroll_locked = false;

    ensure_scrollback_started();

    /* CRITICAL: Enable hardware cursor and position */
    enable_cursor();
    hw_cursor_row = 0;
    hw_cursor_col = 0;
    update_cursor();

    /* Render initial viewport */
    terminal_render_viewport();
}

/* Clear the screen and reset both VGA and scrollback */
void terminal_clear(void)
{
    /* keep current color as-is */

    /* clear scrollback */
    scrollback_count = 0;
    logical_row = 0;
    logical_col = 0;
    viewport_top = 0;
    scroll_locked = false;
    ensure_scrollback_started();

    /* clear VGA */
    const uint16_t blank = vga_entry(' ', terminal_color);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = blank;
    }

    hw_cursor_row = 0;
    hw_cursor_col = 0;
    update_cursor();
}

/* Move to next line (like pressing Enter) */
void terminal_newline(void)
{
    /* Finish current line by ensuring it exists in scrollback (it always should) */
    if (logical_col >= VGA_WIDTH) logical_col = VGA_WIDTH - 1;

    /* Move logical cursor to new line */
    logical_col = 0;
    logical_row++;

    if (logical_row >= scrollback_count) {
        scrollback_append_blank_line();
    }

    /* If user is following, ensure viewport shows latest */
    if (!scroll_locked) {
        terminal_resume_follow();
    } else {
        /* when locked (viewing older output) we don't auto-follow */
        terminal_render_viewport();
    }
}

/* Write a single character to the screen (and scrollback) */
void terminal_putchar(char c)
{
    ensure_scrollback_started();

    /* Handle special characters */
    switch (c)
    {
    case '\n': /* Newline */
        terminal_newline();
        return;

    case '\r': /* Carriage return */
        logical_col = 0;
        if (!scroll_locked) terminal_resume_follow(); else terminal_render_viewport();
        return;

    case '\b': /* Backspace */
        if (logical_col > 0)
        {
            logical_col--;
            scrollback[logical_row][logical_col] = vga_entry(' ', terminal_color);
        }
        else if (logical_row > 0)
        {
            /* move to end of previous line */
            logical_row--;
            logical_col = VGA_WIDTH - 1;
            scrollback[logical_row][logical_col] = vga_entry(' ', terminal_color);
        }
        if (!scroll_locked) terminal_resume_follow(); else terminal_render_viewport();
        return;

    case '\t': /* Tab - align to next 4-column boundary */
        {
            size_t next = (logical_col + 4) & ~(4 - 1);
            if (next >= VGA_WIDTH) {
                terminal_newline();
            } else {
                /* fill with spaces */
                while (logical_col < next) {
                    scrollback[logical_row][logical_col++] = vga_entry(' ', terminal_color);
                }
                if (!scroll_locked) terminal_resume_follow(); else terminal_render_viewport();
            }
        }
        return;
    }

    /* Ignore non-printable characters */
    if ((unsigned char)c < 32 || (unsigned char)c > 126)
        return;

    /* Regular printable character - write to logical buffer */
    scrollback[logical_row][logical_col] = vga_entry((unsigned char)c, terminal_color);

    logical_col++;
    if (logical_col >= VGA_WIDTH) {
        logical_col = 0;
        logical_row++;
        if (logical_row >= scrollback_count) scrollback_append_blank_line();
    }

    /* If the user is following (viewport_top == 0), update VGA row (fast path).
     * Otherwise we only update the scrollback buffer; user will see old content until they resume follow. */
    if (!scroll_locked) {
        /* compute top index */
        size_t start = (scrollback_count > VGA_HEIGHT) ? (scrollback_count - VGA_HEIGHT) : 0;
        /* determine the VGA row where logical_row should appear */
        if (logical_row >= start && logical_row < start + VGA_HEIGHT) {
            size_t row_in_vga = logical_row - start;
            terminal_update_vga_row(row_in_vga, logical_row);
        } else {
            /* logical_line sits beyond the immediate viewport; render full viewport to be safe */
            terminal_render_viewport();
        }
        /* place hw cursor at new location */
        hw_cursor_row = (logical_row >= start) ? (logical_row - start) : (VGA_HEIGHT - 1);
        hw_cursor_col = logical_col;
        update_cursor();
    } else {
        /* user scrolled up - do not change visible VGA contents */
        terminal_render_viewport();
    }
}

/* Write a null-terminated string to the screen */
void terminal_writestring(const char *data)
{
    if (!data) return;
    while (*data) terminal_putchar(*data++);
}

/* Change current drawing color */
void terminal_setcolor(uint8_t color)
{
    terminal_color = color;
}

/* Numeric helpers (unchanged) */
void terminal_write_dec(uint32_t n)
{
    char buffer[12]; // enough for 32-bit uint
    itoa((int)n, buffer);
    terminal_writestring(buffer);
}

void terminal_write_hex(uint32_t value)
{
    char buffer[11]; // "0x" + 8 hex digits + null
    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < 8; i++)
    {
        uint8_t nibble = (value >> ((7 - i) * 4)) & 0xF;
        buffer[2 + i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }

    buffer[10] = '\0';
    terminal_writestring(buffer);
}
