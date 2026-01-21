#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

/* Heap statistics structure */
typedef struct {
    size_t total_pages;
    size_t total_bytes;
    size_t used_bytes;
    size_t free_bytes;
} heap_stats_t;

/* Heap initialization and management */
void heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);
void* kmalloc_aligned(size_t size, size_t alignment);

heap_stats_t heap_get_stats(void);

/* Convenience macros */
#define KALLOC(type) ((type*)kmalloc(sizeof(type)))
#define KALLOC_ARRAY(type, count) ((type*)kmalloc(sizeof(type) * (count)))
#define KFREE(ptr) do { kfree(ptr); (ptr) = NULL; } while(0)

#endif /* HEAP_H */