/* drivers/terminal.c - VGA text mode display driver
 * 
 * VGA text mode is a simple display mode where the screen is divided into
 * 80 columns x 25 rows of characters. Each character takes 2 bytes:
 * - Byte 0: ASCII character code
 * - Byte 1: Attribute byte (4 bits background, 4 bits foreground color)
 * 
 * The VGA buffer is memory-mapped at physical address 0xB8000.
 * Writing to this memory directly updates what appears on screen.
 * 
 * Design decisions:
 * - Simple scrolling implementation (could be optimized)
 * - Row/column tracking for cursor position
 * - Color management for pretty output
 */

#include "../kernel/kernel.h"

/* VGA text buffer - memory-mapped I/O at fixed address
 * This is a hardware-defined location, not something we choose */
static uint16_t* const VGA_MEMORY = (uint16_t*)0xB8000;

/* Current cursor position - where next character will be written */
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;

/* Helper: Combine foreground and background colors into VGA attribute byte */
uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | (bg << 4);
}

/* Helper: Combine character and color into a VGA entry (16-bit value) */
uint16_t vga_entry(unsigned char c, uint8_t color) {
    /* Ensure character is valid ASCII (0-127) to prevent weird characters */
    return (uint16_t)(c & 0x7F) | ((uint16_t)color << 8);
}

/* Scroll the terminal up by one line
 * 
 * How it works:
 * 1. Copy each line to the line above it (line 1 -> line 0, etc.)
 * 2. Clear the bottom line
 * 3. Move cursor to start of bottom line
 * 
 * This is called when we run out of screen space at the bottom.
 * Performance note: This copies 24 lines * 80 chars = 1920 words.
 * Could be optimized with hardware scrolling, but this is simple.
 */
static void terminal_scroll(void) {
    /* Copy each line to the previous line */
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t src = (y + 1) * VGA_WIDTH + x;
            const size_t dst = y * VGA_WIDTH + x;
            VGA_MEMORY[dst] = VGA_MEMORY[src];
        }
    }
    
    /* Clear the bottom line */
    const uint16_t blank = vga_entry(' ', terminal_color);
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        VGA_MEMORY[index] = blank;
    }
    
    /* Move cursor to start of bottom line */
    terminal_row = VGA_HEIGHT - 1;
    terminal_column = 0;
}

/* Initialize terminal - clear screen and set up initial state
 * 
 * Called once during kernel boot. Sets up:
 * - Default color scheme (white on black for better readability)
 * - Cursor position (top-left)
 * - Clear the entire screen (IMPORTANT: wipes any garbage/BIOS text)
 */
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    /* Changed to white on black for better compatibility */
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    /* CRITICAL: Clear entire screen to remove any garbage/BIOS text
     * This fixes the "strange characters" issue from leftover data */
    const uint16_t blank = vga_entry(' ', terminal_color);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = blank;
    }
}

/* Change current drawing color
 * 
 * All subsequent characters will be drawn in this color until
 * it's changed again. Used for syntax highlighting, warnings, etc.
 */
void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

/* Clear the screen and reset cursor to top-left
 * 
 * Used by the 'clear' command in the shell
 */
void terminal_clear(void) {
    const uint16_t blank = vga_entry(' ', terminal_color);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = blank;
    }
    terminal_row = 0;
    terminal_column = 0;
}

/* Move to next line (like pressing Enter)
 * 
 * Resets column to 0 and increments row. If we're at the bottom,
 * scroll the screen up.
 */
void terminal_newline(void) {
    terminal_column = 0;
    if (++terminal_row >= VGA_HEIGHT) {
        terminal_scroll();
    }
}

/* Write a single character to the screen
 * 
 * Special characters:
 * - '\n' (newline): Move to next line
 * - '\r' (carriage return): Move to start of current line  
 * - '\b' (backspace): Delete previous character
 * - '\t' (tab): Move to next tab stop (every 4 columns)
 * 
 * Regular characters: Just display them and advance cursor
 * 
 * This is the core output function - everything else builds on this.
 */
void terminal_putchar(char c) {
    /* Handle special characters */
    switch (c) {
        case '\n':  /* Newline */
            terminal_newline();
            return;
            
        case '\r':  /* Carriage return */
            terminal_column = 0;
            return;
            
        case '\b':  /* Backspace */
            if (terminal_column > 0) {
                terminal_column--;
                /* Replace with space to "erase" */
                const size_t index = terminal_row * VGA_WIDTH + terminal_column;
                VGA_MEMORY[index] = vga_entry(' ', terminal_color);
            }
            return;
            
        case '\t':  /* Tab - align to next 4-column boundary */
            terminal_column = (terminal_column + 4) & ~(4 - 1);
            if (terminal_column >= VGA_WIDTH) {
                terminal_newline();
            }
            return;
    }
    
    /* Ignore non-printable characters (except those handled above) */
    if (c < 32 || c > 126) {
        return;  /* Skip control characters and extended ASCII */
    }
    
    /* Regular printable character - write to screen */
    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    VGA_MEMORY[index] = vga_entry(c, terminal_color);
    
    /* Advance cursor */
    if (++terminal_column >= VGA_WIDTH) {
        terminal_newline();
    }
}

/* Write a null-terminated string to the screen
 * 
 * This is what you'll use most often for output.
 * Just calls terminal_putchar() for each character.
 */
void terminal_writestring(const char* data) {
    if (!data) return;  /* Safety check for null pointer */
    
    while (*data) {
        terminal_putchar(*data++);
    }
}