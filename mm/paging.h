#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "../kernel/kernel.h"   // for PAGE_SIZE

#define PAGE_PRESENT 0x1
#define PAGE_RW      0x2
#define PAGE_USER    0x4

/* 1024 entries per page table or directory, 4 KB aligned */
extern uint32_t page_directory[1024] __attribute__((aligned(4096)));
extern uint32_t page_tables[32][1024] __attribute__((aligned(4096)));

/* Initialize paging with identity mapping for first MEMORY_LIMIT */
void paging_init(void);

#endif
