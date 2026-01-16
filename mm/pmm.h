#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096
#define MEMORY_LIMIT 0x8000000 // 128MB

void pmm_init(uint32_t mem_size);
void* pmm_alloc_block(void);
void pmm_free_block(void* addr);

/* Region management */
void pmm_init_region(void* base, size_t size);
void pmm_deinit_region(void* base, size_t size);

/* Statistics */
uint32_t pmm_get_used_blocks(void);
uint32_t pmm_get_free_blocks(void);
uint32_t pmm_get_total_blocks(void);

#endif
