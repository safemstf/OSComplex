#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* ====================================================================
 * GDT CONSTANTS
 * ==================================================================== */

#define GDT_ENTRIES 6

/* Access byte flags */
#define GDT_PRESENT  0x80

#define GDT_RING0    0x00
#define GDT_RING3    0x60

#define GDT_CODE     0x1A    /* Executable, readable */
#define GDT_DATA     0x12    /* Writable */
#define GDT_TSS      0x09    /* 32-bit TSS (available) */

/* Granularity byte flags */
#define GDT_4K_GRAN  0x80
#define GDT_32BIT    0x40

/* Segment selectors */
#define KERNEL_CS    0x08
#define KERNEL_DS    0x10
#define USER_CS      0x18
#define USER_DS      0x20
#define TSS_SEL      0x28

/* ====================================================================
 * GDT ENTRY STRUCTURES
 * ==================================================================== */

/* One GDT entry (8 bytes) */
struct gdt_entry
{
    uint16_t limit_low;     /* Limit bits 0-15 */
    uint16_t base_low;      /* Base bits 0-15 */
    uint8_t  base_middle;   /* Base bits 16-23 */
    uint8_t  access;        /* Access flags */
    uint8_t  granularity;   /* Granularity + limit bits 16-19 */
    uint8_t  base_high;     /* Base bits 24-31 */
} __attribute__((packed));

/* GDTR structure */
struct gdt_ptr
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* ====================================================================
 * FUNCTION PROTOTYPES
 * ==================================================================== */

/* Initialize GDT */
void gdt_init(void);

/* Set individual GDT entry */
void gdt_set_gate(int num, uint32_t base, uint32_t limit,
                  uint8_t access, uint8_t gran);

/* Install TSS entry */
void gdt_set_tss(uint32_t tss_base, uint32_t tss_limit);

/* Assembly function to load GDT */
extern void gdt_flush(uint32_t gdt_ptr);

#endif /* GDT_H */
