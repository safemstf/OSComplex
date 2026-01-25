/* mm/vmm.h - Virtual Memory Manager
 * 
 * Manages virtual address spaces and page mappings.
 */

#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include "paging.h"  /* For access to page_directory */

/* Page flags */
#define VMM_PRESENT       0x01
#define VMM_WRITE         0x02
#define VMM_USER          0x04
#define VMM_WRITETHROUGH  0x08
#define VMM_CACHEDISABLE  0x10
#define VMM_ACCESSED      0x20
#define VMM_DIRTY         0x40
#define VMM_PAGESIZE      0x80
#define VMM_GLOBAL        0x100

/* Memory layout - constants from kernel.h */
#define KERNEL_BASE        0xC0000000
/* KERNEL_HEAP_START and KERNEL_HEAP_END defined in kernel.h */

#define USER_BASE          0x00000000
#define USER_HEAP_START    0x10000000
#define USER_STACK_TOP     0xBFFFFFFF
#define USER_STACK_SIZE    0x00100000  /* 1MB */
#define USER_STACK_BOTTOM  (USER_STACK_TOP - USER_STACK_SIZE)

/* Virtual memory region */
struct vmm_region {
    uint32_t start;
    uint32_t end;
    uint32_t flags;
    struct vmm_region* next;
};

/* Virtual address space */
struct vmm_address_space {
    uint32_t* page_dir;  /* Pointer to page directory */
    struct vmm_region* regions;
    uint32_t ref_count;
    struct vmm_address_space* next;
};

/* Current address space */
extern struct vmm_address_space* vmm_current_as;

/* Initialization */
void vmm_init(void);

/* Address space management */
struct vmm_address_space* vmm_create_as(void);
void vmm_destroy_as(struct vmm_address_space* as);
void vmm_switch_as(struct vmm_address_space* as);

/* Mapping functions */
void vmm_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);
void vmm_unmap_page(uint32_t virt_addr);
int vmm_map_range(uint32_t virt_start, uint32_t phys_start, size_t size, uint32_t flags);
int vmm_unmap_range(uint32_t virt_start, size_t size);

/* Allocation functions */
void* vmm_alloc_page(uint32_t flags);
void vmm_free_page(void* virt_addr);
void* vmm_alloc_pages(size_t count, uint32_t flags);
void vmm_free_pages(void* virt_addr, size_t count);

void vmm_map_page_in_as(struct vmm_address_space *as,
                        uint32_t virt_addr,
                        uint32_t phys_addr,
                        uint32_t flags);

void vmm_unmap_page_in_as(struct vmm_address_space *as,
                          uint32_t virt_addr);


/* Query functions */
uint32_t vmm_virt_to_phys(uint32_t virt_addr);
uint32_t vmm_get_flags(uint32_t virt_addr);
int vmm_is_mapped(uint32_t virt_addr);

#endif /* VMM_H */