/* kernel/kernel.h - Core kernel definitions and function prototypes
 * 
 * This header defines the fundamental types, constants, and interfaces
 * used throughout the LinuxComplex kernel. It serves as the central
 * contract between different kernel subsystems.
 * 
 * Design Philosophy:
 * - Keep interfaces clean and minimal
 * - Use standard types where possible (stdint.h, stddef.h)
 * - Document all public APIs
 */

#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ==================================================================
 * VGA TEXT MODE SUBSYSTEM
 * ==================================================================
 * VGA text mode is our initial output method. It's simple, always 
 * available, and doesn't require complex graphics initialization.
 * The VGA buffer lives at physical address 0xB8000 and is 80x25 chars.
 */

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* VGA hardware color palette (16 colors available) */
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,  /* This is "yellow" in VGA */
    VGA_COLOR_WHITE = 15,
};

/* VGA helper functions - implemented in terminal.c */
uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg);
uint16_t vga_entry(unsigned char uc, uint8_t color);

/* Terminal/VGA functions */
void terminal_initialize(void);
void terminal_setcolor(uint8_t color);
void terminal_putchar(char c);
void terminal_writestring(const char* data);
void terminal_clear(void);
void terminal_newline(void);

/* ==================================================================
 * PORT I/O SUBSYSTEM
 * ==================================================================
 * x86 has a separate I/O address space accessed via IN/OUT instructions.
 * These are used to communicate with hardware devices like:
 * - PIC (Programmable Interrupt Controller)
 * - Keyboard controller
 * - Timer
 * - Serial ports, etc.
 */

/* Read a byte from an I/O port */
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* Write a byte to an I/O port */
static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

/* ==================================================================
 * INTERRUPT HANDLING SUBSYSTEM
 * ==================================================================
 * Interrupts are how hardware devices (keyboard, timer, disk) notify
 * the CPU that something needs attention. The CPU uses the IDT
 * (Interrupt Descriptor Table) to find the handler for each interrupt.
 * 
 * Intel reserves interrupts 0-31 for CPU exceptions (divide by zero,
 * page faults, etc.). We remap the PIC to use interrupts 32-47 for
 * hardware devices to avoid conflicts.
 */

#define IDT_ENTRIES 256

/* IDT entry structure - describes one interrupt handler
 * This is the x86 protected mode format (8 bytes per entry)
 */
struct idt_entry {
    uint16_t base_low;   /* Lower 16 bits of handler address */
    uint16_t selector;   /* Kernel code segment selector (0x08) */
    uint8_t  zero;       /* Always 0 */
    uint8_t  flags;      /* Type and attributes */
    uint16_t base_high;  /* Upper 16 bits of handler address */
} __attribute__((packed));

/* IDT pointer structure - tells CPU where IDT is located */
struct idt_ptr {
    uint16_t limit;      /* Size of IDT - 1 */
    uint32_t base;       /* Address of IDT */
} __attribute__((packed));

/* Interrupt handler function type */
typedef void (*interrupt_handler_t)(void);

/* IDT management functions */
void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags);

/* IRQ handler registration */
void irq_install_handler(uint8_t irq, interrupt_handler_t handler);
void irq_uninstall_handler(uint8_t irq);

/* ==================================================================
 * PIC (PROGRAMMABLE INTERRUPT CONTROLLER)
 * ==================================================================
 * The 8259 PIC manages hardware interrupts. We have two PICs cascaded:
 * - Master PIC: handles IRQ 0-7
 * - Slave PIC: handles IRQ 8-15 (connected to master's IRQ 2)
 * 
 * We remap them because by default they conflict with CPU exceptions.
 */

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI      0x20  /* End Of Interrupt command */

/* IRQ numbers (hardware interrupts) */
#define IRQ_TIMER    0
#define IRQ_KEYBOARD 1

/* Remapped interrupt numbers (IRQ0 maps to INT 32, etc.) */
#define INT_TIMER    32
#define INT_KEYBOARD 33

void pic_init(void);
void pic_send_eoi(uint8_t irq);

/* ==================================================================
 * KEYBOARD SUBSYSTEM
 * ==================================================================
 * PS/2 keyboard interface. The keyboard sends scancodes when keys
 * are pressed/released. We convert these to ASCII for display.
 */

void keyboard_init(void);
void keyboard_handler(void);

/* ==================================================================
 * STRING UTILITIES
 * ==================================================================
 * We can't use standard library (it assumes an OS exists!), so we
 * implement our own basic string functions.
 */

size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
void* memset(void* dest, int val, size_t len);
void* memcpy(void* dest, const void* src, size_t len);

/* ==================================================================
 * SHELL/COMMAND INTERFACE
 * ==================================================================
 * Simple command-line interface for user interaction
 */

void shell_init(void);
void shell_run(void);

/* ==================================================================
 * AI SUBSYSTEM (Phase 1)
 * ==================================================================
 * Simple pattern recognition and command prediction system.
 * Phase 1: Learn command frequency and suggest completions
 * Future: More sophisticated NLP and context awareness
 */

#define AI_MAX_COMMANDS 32
#define AI_MAX_CMD_LEN 64

/* AI command tracking structure */
struct ai_command_stats {
    char command[AI_MAX_CMD_LEN];
    uint32_t frequency;      /* How many times this command was used */
    uint32_t last_used;      /* Timestamp (in ticks) when last used */
    uint32_t success_rate;   /* Percentage of successful executions */
};

void ai_init(void);
void ai_learn_command(const char* cmd, bool success);
const char* ai_predict_command(const char* prefix);
void ai_show_suggestions(const char* partial);
void ai_show_stats(void);

#endif /* KERNEL_H */