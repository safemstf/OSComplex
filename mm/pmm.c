#include "pmm.h"
#include "../lib/string.h"
#include "../kernel/kernel.h"

/*
 * Each bit represents one 4KB page.
 * MEMORY_LIMIT = 128MB → 32768 pages → 1024 uint32_t entries
 */

static uint32_t pmm_bitmap[MEMORY_LIMIT / PAGE_SIZE / 32];
static uint32_t used_blocks = 0;
static uint32_t max_blocks  = 0;

/* --- Bitmap helpers --- */

static inline void bitmap_set(uint32_t bit) {
    pmm_bitmap[bit >> 5] |=  (1 << (bit & 31));
}

static inline void bitmap_unset(uint32_t bit) {
    pmm_bitmap[bit >> 5] &= ~(1 << (bit & 31));
}

static inline int bitmap_test(uint32_t bit) {
    return pmm_bitmap[bit >> 5] & (1 << (bit & 31));
}

/* --- PMM core --- */

void pmm_init(uint32_t mem_size) {
    max_blocks  = mem_size / PAGE_SIZE;
    used_blocks = max_blocks;               // EVERYTHING USED INITIALLY

    memset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));
}

/* Allocate a single 4KB physical page */
void* pmm_alloc_block() {
    if (used_blocks >= max_blocks)
        return 0;

    for (uint32_t i = 0; i < max_blocks; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_blocks++;
            return (void*)(i * PAGE_SIZE);  // physical address
        }
    }
    return 0;
}

/* Free a single 4KB physical page */
void pmm_free_block(void* addr) {
    uint32_t bit = (uint32_t)addr / PAGE_SIZE;

    if (bit >= max_blocks)
        return;

    if (bitmap_test(bit)) {
        bitmap_unset(bit);
        used_blocks--;
    }
}

/* --- Region management --- */

/* Mark region as FREE */
void pmm_init_region(void* base, size_t size) {
    uint32_t start = ((uint32_t)base + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t blocks = size / PAGE_SIZE;

    for (uint32_t i = 0; i < blocks; i++) {
        uint32_t bit = start + i;
        if (bit < max_blocks && bitmap_test(bit)) {
            bitmap_unset(bit);
            used_blocks--;
        }
    }
}

/* Mark region as USED (reserved) */
void pmm_deinit_region(void* base, size_t size) {
    uint32_t start = (uint32_t)base / PAGE_SIZE;
    uint32_t blocks = size / PAGE_SIZE;

    for (uint32_t i = 0; i < blocks; i++) {
        uint32_t bit = start + i;
        if (bit < max_blocks && !bitmap_test(bit)) {
            bitmap_set(bit);
            used_blocks++;
        }
    }
}

/* --- Stats --- */

uint32_t pmm_get_used_blocks() {
    return used_blocks;
}

uint32_t pmm_get_free_blocks() {
    return max_blocks - used_blocks;
}

uint32_t pmm_get_total_blocks(void) {
    return max_blocks;
}