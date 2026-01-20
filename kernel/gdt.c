/* kernel/gdt.c - Global Descriptor Table and TSS for User Mode
 * 
 * Sets up proper segmentation for Ring 0 (kernel) and Ring 3 (user)
 */

#include "kernel.h"
#include "tss.h"


/* GDT entry structure */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/* GDT pointer structure */
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));


/* GDT with 6 entries:
 * 0: Null descriptor
 * 1: Kernel code segment (0x08)
 * 2: Kernel data segment (0x10)
 * 3: User code segment   (0x18)
 * 4: User data segment   (0x20)
 * 5: TSS                 (0x28)
 */
static struct gdt_entry gdt[6];
static struct gdt_ptr gdt_ptr;
static struct tss_entry tss;

/* Set a GDT entry */
static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;
    
    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

/* Initialize GDT and TSS */
void gdt_init(void) {
    gdt_ptr.limit = (sizeof(struct gdt_entry) * 6) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;
    
    /* Null descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);
    
    /* Kernel code segment: base=0, limit=4GB, access=9A (exec/read), gran=CF (4KB pages) */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    /* Kernel data segment: base=0, limit=4GB, access=92 (read/write), gran=CF */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    
    /* User code segment: base=0, limit=4GB, access=FA (exec/read, DPL=3), gran=CF */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    
    /* User data segment: base=0, limit=4GB, access=F2 (read/write, DPL=3), gran=CF */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    
    /* TSS: access=89 (present, executable, accessed), gran=00 (byte granularity) */
    memset(&tss, 0, sizeof(tss));
    tss.ss0 = 0x10;  /* Kernel data segment */
    tss.esp0 = 0;    /* Will be set when entering user mode */
    
    uint32_t tss_base = (uint32_t)&tss;
    uint32_t tss_limit = sizeof(tss);
    gdt_set_gate(5, tss_base, tss_limit, 0x89, 0x00);
    
    /* Load GDT */
    __asm__ volatile("lgdt %0" : : "m"(gdt_ptr));
    
    /* Reload segment registers */
    __asm__ volatile(
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
        : : : "eax"
    );
    
    /* Load TSS */
    __asm__ volatile("ltr %%ax" : : "a"(0x2B));  /* 0x28 | 3 (RPL=3) */
}

/* Install TSS descriptor into the GDT
 *
 * This function is REQUIRED because it is declared in gdt.h
 * and called from tss.c. It must NOT be static.
 */
void gdt_set_tss(uint32_t tss_base, uint32_t tss_limit)
{
    /* GDT entry 5 = TSS (selector 0x28)
     *
     * Access byte 0x89:
     *  - Present
     *  - Ring 0
     *  - System segment
     *  - Available 32-bit TSS
     *
     * Granularity must be 0 for TSS (byte granularity)
     */
    gdt_set_gate(
        5,              /* GDT index */
        tss_base,       /* Base address of TSS */
        tss_limit,      /* Size of TSS */
        0x89,           /* Access byte */
        0x00            /* Granularity */
    );
}
