#include "paging.h"

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t page_tables[32][1024] __attribute__((aligned(4096)));

void paging_init(void) {
    // Map first 128 MB = 32768 pages → 32 page tables (32*1024 = 32768)
    for (int t = 0; t < 32; t++) {
        for (int i = 0; i < 1024; i++) {
            uint32_t phys_addr = (t * 1024 + i) * PAGE_SIZE;
            page_tables[t][i] = phys_addr | PAGE_PRESENT | PAGE_RW;
        }
        page_directory[t] = ((uint32_t)page_tables[t]) | PAGE_PRESENT | PAGE_RW;
    }

    // Other entries not present
    for (int t = 32; t < 1024; t++)
        page_directory[t] = 0;

    // Load CR3 with page directory
    asm volatile("mov %0, %%cr3" :: "r"(page_directory));

    // Enable paging (CR0 PG bit)
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;  // PG bit
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}
