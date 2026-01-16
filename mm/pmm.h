#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>


void pmm_init(uint32_t mem_size);
void* pmm_alloc_block(void);
void pmm_free_block(void* addr);
void pmm_init_region(void* base, size_t size);
void pmm_deinit_region(void* base, size_t size);
uint32_t pmm_get_used_blocks(void);
uint32_t pmm_get_free_blocks(void);
uint32_t pmm_get_total_blocks(void);

#endif