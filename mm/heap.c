/* mm/heap.c - Kernel heap using PMM page allocation
 *
 * This implements a simple heap that allocates whole pages from PMM
 * for large allocations, and manages sub-page blocks for small allocations.
 */

#include "heap.h"
#include "pmm.h"
#include "../lib/string.h"
#include "../kernel/kernel.h"

/* Heap block structure for small allocations (< PAGE_SIZE/2) */
typedef struct heap_block
{
    size_t size; /* Size of this block (excluding header) */
    struct heap_block *next;
    int free;       /* 1 if free, 0 if allocated */
    uint32_t magic; /* Magic number for integrity check */
} heap_block_t;

#define HEAP_MAGIC 0xDEADBEEF
#define HEAP_INITIAL_PAGES 4                      /* Start with 16KB heap */
#define HEAP_MAX_PAGES 256                        /* Maximum heap size: 1MB */
#define MIN_BLOCK_SIZE (sizeof(heap_block_t) + 8) /* Minimum allocation */

/* Heap structure tracking all allocated pages */
static struct
{
    void *pages[HEAP_MAX_PAGES];
    size_t page_count;
    heap_block_t *free_list;
} heap;

/* Align size to 8 bytes */
static inline size_t align_size(size_t size)
{
    return (size + 7) & ~7;
}

/* Get the total size including header */
static inline size_t total_size(size_t size)
{
    return sizeof(heap_block_t) + align_size(size);
}

/* Initialize the kernel heap */
void heap_init(void)
{
    memset(&heap, 0, sizeof(heap));
    heap.free_list = NULL;

    /* Allocate initial pages */
    for (int i = 0; i < HEAP_INITIAL_PAGES; i++)
    {
        void *page = pmm_alloc_block();
        if (!page)
        {
            terminal_writestring("[HEAP] ERROR: Could not allocate initial page\n");
            return;
        }

        heap.pages[heap.page_count++] = page;

        /* Set up first block in this page */
        heap_block_t *block = (heap_block_t *)page;
        block->size = PAGE_SIZE - sizeof(heap_block_t);
        block->free = 1;
        block->next = heap.free_list;
        block->magic = HEAP_MAGIC;
        heap.free_list = block;
    }

    terminal_writestring("[HEAP] Initialized with ");
    char buf[16];
    itoa(heap.page_count, buf);
    terminal_writestring(buf);
    terminal_writestring(" pages\n");
}

/* Add a new page to the heap */
static int heap_grow(void)
{
    if (heap.page_count >= HEAP_MAX_PAGES)
    {
        return 0;
    }

    void *page = pmm_alloc_block();
    if (!page)
    {
        return 0;
    }

    heap.pages[heap.page_count++] = page;

    /* Add new page to free list */
    heap_block_t *block = (heap_block_t *)page;
    block->size = PAGE_SIZE - sizeof(heap_block_t);
    block->free = 1;
    block->next = heap.free_list;
    block->magic = HEAP_MAGIC;
    heap.free_list = block;

    return 1;
}

/* Split a block if it's too large */
static void split_block(heap_block_t *block, size_t size)
{
    size_t remaining = block->size - size;

    /* Only split if remaining is large enough for another block */
    if (remaining >= MIN_BLOCK_SIZE)
    {
        heap_block_t *new_block = (heap_block_t *)((char *)block + sizeof(heap_block_t) + size);
        new_block->size = remaining - sizeof(heap_block_t);
        new_block->free = 1;
        new_block->next = block->next;
        new_block->magic = HEAP_MAGIC;

        block->size = size;
        block->next = new_block;
    }
}

/* Merge adjacent free blocks */
static void merge_blocks(void)
{
    heap_block_t *curr = heap.free_list;

    while (curr)
    {
        heap_block_t *next = curr->next;

        /* Check if we can merge with next block */
        if (next && curr->free && next->free &&
            (char *)next == (char *)curr + sizeof(heap_block_t) + curr->size)
        {
            /* Merge curr and next */
            curr->size += sizeof(heap_block_t) + next->size;
            curr->next = next->next;
            /* Continue checking from current block (might merge with next-next) */
            continue;
        }

        curr = next;
    }
}

/* Allocate memory from the heap */
void *kmalloc(size_t size)
{
    if (size == 0)
    {
        return NULL;
    }

    size_t needed = total_size(size);

    /* Large allocation: get whole page(s) */
    if (needed >= PAGE_SIZE / 2)
    {
        size_t pages_needed = (needed + PAGE_SIZE - 1) / PAGE_SIZE;

        /* Allocate contiguous physical pages */
        void *first_page = pmm_alloc_block();
        if (!first_page)
        {
            return NULL;
        }

        /* Allocate remaining pages (must be contiguous for this simple allocator) */
        /* Note: PMM allocates linearly, so consecutive calls give contiguous pages */
        for (size_t i = 1; i < pages_needed; i++)
        {
            void *page = pmm_alloc_block();
            if (!page)
            {
                /* Failed - free what we allocated */
                for (size_t j = 0; j < i; j++)
                {
                    pmm_free_block((void *)((char *)first_page + j * PAGE_SIZE));
                }
                return NULL;
            }
            /* Verify contiguity (PMM should give us consecutive pages) */
            if ((char *)page != (char *)first_page + i * PAGE_SIZE)
            {
                /* Not contiguous - for now just continue, it's still usable via identity mapping */
                /* A proper implementation would need a contiguous allocator */
            }
        }

        /* Store size in first 4 bytes for kfree to know it's a large allocation */
        size_t *header = (size_t *)first_page;
        *header = pages_needed * PAGE_SIZE;
        return (void *)((size_t *)first_page + 1);
    }

    /* Small allocation: use block allocator */
    heap_block_t *prev = NULL;
    heap_block_t *curr = heap.free_list;

    while (curr)
    {
        if (curr->free && curr->size >= size)
        {
            /* Found a suitable block */
            split_block(curr, align_size(size));
            curr->free = 0;

            /* Remove from free list */
            if (prev)
            {
                prev->next = curr->next;
            }
            else
            {
                heap.free_list = curr->next;
            }

            return (void *)((char *)curr + sizeof(heap_block_t));
        }
        prev = curr;
        curr = curr->next;
    }

    /* No suitable block found, try to grow heap */
    if (heap_grow())
    {
        return kmalloc(size); /* Try again */
    }

    return NULL;
}

/* Free allocated memory */
void kfree(void *ptr)
{
    if (!ptr)
    {
        return;
    }

    /* Check if it's a large allocation (whole page) */
    size_t *header = (size_t *)((char *)ptr - sizeof(size_t));

    /* Simple heuristic: if the address is page-aligned, assume it's a large allocation */
    if (((uint32_t)header & (PAGE_SIZE - 1)) == 0)
    {
        size_t size = *header;
        size_t pages = size / PAGE_SIZE;

        /* Return all pages to PMM */
        for (size_t i = 0; i < pages; i++)
        {
            void *page = (void *)((char *)header + i * PAGE_SIZE);
            pmm_free_block(page);
        }
        return;
    }

    /* Small block allocation */
    heap_block_t *block = (heap_block_t *)((char *)ptr - sizeof(heap_block_t));

    if (block->magic != HEAP_MAGIC)
    {
        terminal_writestring("[HEAP] ERROR: Invalid block magic\n");
        return;
    }

    block->free = 1;
    block->next = heap.free_list;
    heap.free_list = block;

    /* Try to merge with adjacent free blocks */
    merge_blocks();
}

/* Allocate aligned memory */
void *kmalloc_aligned(size_t size, size_t alignment)
{
    /* For page-aligned allocations, just use PMM directly */
    if (alignment == PAGE_SIZE && size == PAGE_SIZE) {
        return pmm_alloc_block();
    }
    
    /* For other alignments, allocate extra space and align manually */
    size_t total_size = size + alignment + sizeof(void*);
    void *ptr = kmalloc(total_size);
    if (!ptr) {
        return NULL;
    }
    
    /* Calculate aligned address */
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned = (addr + sizeof(void*) + alignment - 1) & ~(alignment - 1);
    
    /* Store original pointer before aligned address for kfree */
    *((void**)(aligned - sizeof(void*))) = ptr;
    
    return (void*)aligned;
}

/* Get heap statistics for debugging */
heap_stats_t heap_get_stats(void)
{
    heap_stats_t stats = {0};

    /* Count free/used in small blocks */
    heap_block_t *curr = heap.free_list;
    while (curr)
    {
        if (curr->free)
        {
            stats.free_bytes += curr->size;
        }
        else
        {
            stats.used_bytes += curr->size;
        }
        curr = curr->next;
    }

    stats.total_pages = heap.page_count;
    stats.total_bytes = heap.page_count * PAGE_SIZE;

    return stats;
}