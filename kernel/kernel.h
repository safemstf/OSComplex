/* kernel/kernel.h - Core kernel definitions
 * 
 * Central header for all kernel subsystems.
 * UPDATED: Added MM subsystem headers and interrupt structures.
 */

#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include "syscall.h"

/* Timer */
void timer_init(void);
uint32_t timer_get_ticks(void);
void timer_sleep(uint32_t ms);

/* ================================================================== 
 * CORE CONSTANTS
 * ================================================================== */

#define PAGE_SIZE 4096
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* ==================================================================
 * VGA TEXT MODE
 * ================================================================== */

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
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg);
uint16_t vga_entry(unsigned char uc, uint8_t color);

void terminal_initialize(void);
void terminal_setcolor(uint8_t color);
void terminal_putchar(char c);
void terminal_writestring(const char* data);
void terminal_clear(void);
void terminal_newline(void);

/* ==================================================================
 * PORT I/O
 * ================================================================== */

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

/* ==================================================================
 * INTERRUPT HANDLING
 * ================================================================== */

#define IDT_ENTRIES 256

/* CPU state saved by interrupt handler */
struct registers {
    uint32_t ds;                                       /* Data segment */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  /* pusha */
    uint32_t int_no, err_code;                        /* Interrupt number and error code */
    uint32_t eip, cs, eflags, useresp, ss;            /* Pushed by CPU */
} __attribute__((packed));

/* IDT entry */
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

/* IDT pointer */
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

typedef void (*interrupt_handler_t)(void);

void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags);
void irq_install_handler(uint8_t irq, interrupt_handler_t handler);
void irq_uninstall_handler(uint8_t irq);

/* Exception handlers - declared in isr.c as needed */
/* void page_fault_handler() - handled internally by ISR */

/* ==================================================================
 * PIC (PROGRAMMABLE INTERRUPT CONTROLLER)
 * ================================================================== */

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

#define IRQ_TIMER    0
#define IRQ_KEYBOARD 1

#define INT_TIMER    32
#define INT_KEYBOARD 33

void pic_init(void);
void pic_send_eoi(uint8_t irq);

/* ==================================================================
 * KEYBOARD
 * ================================================================== */

void keyboard_init(void);
void keyboard_handler(void);

/* ==================================================================
 * STRING UTILITIES
 * ================================================================== */

size_t strlen(const char* str);
size_t strnlen(const char* str, size_t maxlen);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
int stricmp(const char* s1, const char* s2);

void* memset(void* dest, int val, size_t len);
void* memcpy(void* dest, const void* src, size_t len);
void* memmove(void* dest, const void* src, size_t len);
int memcmp(const void* s1, const void* s2, size_t n);

char* strchr(const char* s, int c);
void* memchr(const void* s, int c, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);
char* strstr(const char* haystack, const char* needle);
char* strtrim(char* str);

void itoa(int n, char* str);
void utoa(uint32_t n, char* str, int base);

/* Heap-based string functions */
char* strdup(const char* s);
char* strndup(const char* s, size_t n);
char* strconcat(const char* s1, const char* s2);

/* ==================================================================
 * MEMORY MANAGEMENT
 * ================================================================== */

/* PMM - Physical Memory Manager */
#define MEMORY_LIMIT 0x08000000  /* 128MB */

void pmm_init(uint32_t mem_size);
void pmm_init_region(void* base, size_t size);
void pmm_deinit_region(void* base, size_t size);
void* pmm_alloc_block(void);
void pmm_free_block(void* addr);
uint32_t pmm_get_free_block_count(void);

/* Paging */
void paging_init(void);
void paging_enable(void);

/* VMM - Virtual Memory Manager */
#define VMM_PRESENT  0x01
#define VMM_WRITE    0x02
#define VMM_USER     0x04

struct vmm_address_space;
extern struct vmm_address_space* vmm_current_as;

void vmm_init(void);
void vmm_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);
void vmm_unmap_page(uint32_t virt_addr);
void* vmm_alloc_page(uint32_t flags);
void vmm_free_page(void* virt_addr);
uint32_t vmm_virt_to_phys(uint32_t virt_addr);
int vmm_is_mapped(uint32_t virt_addr);

/* Heap */
#define KERNEL_HEAP_START 0xC0400000
#define KERNEL_HEAP_SIZE  0x00400000
#define KERNEL_HEAP_END   (KERNEL_HEAP_START + KERNEL_HEAP_SIZE)

void heap_init(void);
void* kmalloc(size_t size);
void* kmalloc_a(size_t size);
void kfree(void* ptr);

/* ==================================================================
 * SHELL
 * ================================================================== */

void shell_init(void);
void shell_run(void);

/* ==================================================================
 * VIRTUAL FILE SYSTEM
 * ================================================================== */

void vfs_init(void);

/* ==================================================================
 * AI SUBSYSTEM
 * ================================================================== */

#define AI_MAX_COMMANDS 32
#define AI_MAX_CMD_LEN 64

struct ai_command_stats {
    char command[AI_MAX_CMD_LEN];
    uint32_t frequency;
    uint32_t last_used;
    uint32_t success_rate;
};

void ai_init(void);
void ai_learn_command(const char* cmd, bool success);
const char* ai_predict_command(const char* prefix);
void ai_show_suggestions(const char* partial);
void ai_show_stats(void);

#endif /* KERNEL_H */