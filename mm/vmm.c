/* mm/vmm.c - Virtual Memory Manager
 *
 * FIXED: No kmalloc during init, proper bootstrap sequence
 */

#include "vmm.h"
#include "pmm.h"
#include "../lib/string.h"
#include "../kernel/kernel.h"

/* Global state */
static uint32_t *kernel_page_dir = NULL;
struct vmm_address_space *vmm_current_as = NULL;

/* Static kernel address space for bootstrapping */
static struct vmm_address_space kernel_as_static;
static struct vmm_region kernel_heap_region_static;

/* Helper functions */
static inline uint32_t pd_index(uint32_t virt_addr)
{
    return (virt_addr >> 22) & 0x3FF;
}

static inline uint32_t pt_index(uint32_t virt_addr)
{
    return (virt_addr >> 12) & 0x3FF;
}

static inline uint32_t align_page(uint32_t addr)
{
    return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

/* Forward declaration */
static void vmm_map_page_internal(uint32_t *page_dir, uint32_t virt_addr,
                                  uint32_t phys_addr, uint32_t flags);

/* Get or create page table */
static uint32_t *get_page_table(uint32_t *pd, uint32_t pd_idx, uint32_t flags)
{
    if (!pd)
        return NULL;

    uint32_t pde = pd[pd_idx];

    /* If exists, return it */
    if (pde & VMM_PRESENT)
    {
        return (uint32_t *)(pde & ~0xFFF);
    }

    /* Allocate new page table */
    uint32_t *pt = (uint32_t *)pmm_alloc_block();
    if (!pt)
        return NULL;

    memset(pt, 0, PAGE_SIZE);

    /* Set directory entry */
    pd[pd_idx] = ((uint32_t)pt) | (flags & 0xFFF) | VMM_PRESENT | VMM_WRITE;

    return pt;
}

/* Internal mapping function that takes explicit page directory */
static void vmm_map_page_internal(uint32_t *page_dir, uint32_t virt_addr,
                                  uint32_t phys_addr, uint32_t flags)
{
    if (!page_dir)
        return;

    uint32_t pd_idx = pd_index(virt_addr);
    uint32_t pt_idx = pt_index(virt_addr);

    /* Get or create page table */
    uint32_t *pt = get_page_table(page_dir, pd_idx, flags);
    if (!pt)
        return;

    /* Set page table entry */
    pt[pt_idx] = (phys_addr & ~0xFFF) | (flags & 0xFFF);

    /* Invalidate TLB */
    asm volatile("invlpg (%0)" ::"r"(virt_addr & ~0xFFF) : "memory");
}

/* Initialize VMM - Use existing page directory from paging.c */
void vmm_init(void)
{
    terminal_writestring("[VMM] Initializing virtual memory manager...\n");

    /* CRITICAL FIX: Use the page directory that paging.c already set up
     * instead of creating a new one! */
    extern uint32_t page_directory[1024]; /* From paging.c */
    kernel_page_dir = page_directory;

    terminal_writestring("[VMM] Using existing page directory from paging subsystem\n");

    /* The page directory is already loaded in CR3 by paging_init(),
     * and it already has 128MB identity-mapped, so we're good! */

    /* Set up STATIC kernel address space (no kmalloc!) */
    kernel_as_static.page_dir = kernel_page_dir;
    kernel_as_static.regions = &kernel_heap_region_static;
    kernel_as_static.ref_count = 1;
    kernel_as_static.next = NULL;

    /* Set up STATIC heap region */
    kernel_heap_region_static.start = KERNEL_HEAP_START;
    kernel_heap_region_static.end = KERNEL_HEAP_END;
    kernel_heap_region_static.flags = VMM_PRESENT | VMM_WRITE;
    kernel_heap_region_static.next = NULL;

    /* Set current address space */
    vmm_current_as = &kernel_as_static;

    terminal_writestring("[VMM] Virtual memory manager initialized\n");
}

/* Public mapping function - uses current address space */
void vmm_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags)
{
    if (!vmm_current_as || !vmm_current_as->page_dir)
        return;
    vmm_map_page_internal(vmm_current_as->page_dir, virt_addr, phys_addr, flags);
}

/* Unmap a page */
void vmm_unmap_page(uint32_t virt_addr)
{
    if (!vmm_current_as || !vmm_current_as->page_dir)
        return;

    uint32_t pd_idx = pd_index(virt_addr);
    uint32_t pt_idx = pt_index(virt_addr);

    uint32_t pde = vmm_current_as->page_dir[pd_idx];
    if (!(pde & VMM_PRESENT))
        return;

    uint32_t *pt = (uint32_t *)(pde & ~0xFFF);
    uint32_t old_pte = pt[pt_idx];

    /* Clear entry */
    pt[pt_idx] = 0;

    /* Free physical page if it was allocated */
    if (old_pte & VMM_PRESENT)
    {
        uint32_t phys = old_pte & ~0xFFF;
        if (phys >= 0x100000)
        { /* Don't free reserved low memory */
            pmm_free_block((void *)phys);
        }
    }

    /* Invalidate TLB */
    asm volatile("invlpg (%0)" ::"r"(virt_addr & ~0xFFF) : "memory");
}

/* Map a range of pages */
int vmm_map_range(uint32_t virt_start, uint32_t phys_start, size_t size, uint32_t flags)
{
    if (size == 0)
        return 0;

    uint32_t virt = virt_start & ~0xFFF;
    uint32_t phys = phys_start & ~0xFFF;
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (size_t i = 0; i < pages; i++)
    {
        vmm_map_page(virt + i * PAGE_SIZE, phys + i * PAGE_SIZE, flags);
    }

    return 1;
}

/* Unmap a range */
int vmm_unmap_range(uint32_t virt_start, size_t size)
{
    if (size == 0)
        return 0;

    uint32_t virt = virt_start & ~0xFFF;
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (size_t i = 0; i < pages; i++)
    {
        vmm_unmap_page(virt + i * PAGE_SIZE);
    }

    return 1;
}

/* Allocate a virtual page */
void *vmm_alloc_page(uint32_t flags)
{
    static uint32_t next_virt = KERNEL_HEAP_START;

    /* Find next free virtual address */
    uint32_t virt = next_virt;
    next_virt += PAGE_SIZE;

    /* Allocate physical page */
    void *phys = pmm_alloc_block();
    if (!phys)
        return NULL;

    /* Map it */
    vmm_map_page(virt, (uint32_t)phys, flags);

    /* Zero the page */
    memset((void *)virt, 0, PAGE_SIZE);

    return (void *)virt;
}

/* Free a virtual page */
void vmm_free_page(void *virt_addr)
{
    vmm_unmap_page((uint32_t)virt_addr);
}

/* Allocate multiple pages */
void *vmm_alloc_pages(size_t count, uint32_t flags)
{
    if (count == 0)
        return NULL;

    static uint32_t next_virt = KERNEL_HEAP_START + 0x100000;

    uint32_t base_virt = next_virt;
    next_virt += count * PAGE_SIZE;

    /* Allocate and map each page */
    for (size_t i = 0; i < count; i++)
    {
        void *phys = pmm_alloc_block();
        if (!phys)
        {
            /* Cleanup on failure */
            for (size_t j = 0; j < i; j++)
            {
                vmm_unmap_page(base_virt + j * PAGE_SIZE);
            }
            return NULL;
        }
        vmm_map_page(base_virt + i * PAGE_SIZE, (uint32_t)phys, flags);
        memset((void *)(base_virt + i * PAGE_SIZE), 0, PAGE_SIZE);
    }

    return (void *)base_virt;
}

/* Free multiple pages */
void vmm_free_pages(void *virt_addr, size_t count)
{
    uint32_t virt = (uint32_t)virt_addr;
    for (size_t i = 0; i < count; i++)
    {
        vmm_unmap_page(virt + i * PAGE_SIZE);
    }
}

/* Get physical address from virtual */
uint32_t vmm_virt_to_phys(uint32_t virt_addr)
{
    if (!vmm_current_as || !vmm_current_as->page_dir)
        return 0;

    uint32_t pd_idx = pd_index(virt_addr);
    uint32_t pt_idx = pt_index(virt_addr);

    uint32_t pde = vmm_current_as->page_dir[pd_idx];
    if (!(pde & VMM_PRESENT))
        return 0;

    uint32_t *pt = (uint32_t *)(pde & ~0xFFF);
    uint32_t pte = pt[pt_idx];
    if (!(pte & VMM_PRESENT))
        return 0;

    return (pte & ~0xFFF) | (virt_addr & 0xFFF);
}

/* Get page flags */
uint32_t vmm_get_flags(uint32_t virt_addr)
{
    if (!vmm_current_as || !vmm_current_as->page_dir)
        return 0;

    uint32_t pd_idx = pd_index(virt_addr);
    uint32_t pt_idx = pt_index(virt_addr);

    uint32_t pde = vmm_current_as->page_dir[pd_idx];
    if (!(pde & VMM_PRESENT))
        return 0;

    uint32_t *pt = (uint32_t *)(pde & ~0xFFF);
    return pt[pt_idx] & 0xFFF;
}

/* Check if mapped */
int vmm_is_mapped(uint32_t virt_addr)
{
    return (vmm_get_flags(virt_addr) & VMM_PRESENT) != 0;
}

/* Create new address space (requires heap) */
struct vmm_address_space *vmm_create_as(void)
{
    /* Allocate structure */
    struct vmm_address_space *as = kmalloc(sizeof(struct vmm_address_space));
    if (!as)
        return NULL;

    /* Allocate page directory */
    as->page_dir = (uint32_t *)pmm_alloc_block();
    if (!as->page_dir)
    {
        kfree(as);
        return NULL;
    }
    memset(as->page_dir, 0, PAGE_SIZE);

    /* Copy kernel mappings (upper 256 entries) */
    for (uint32_t i = 768; i < 1024; i++)
    {
        if (kernel_page_dir[i] & VMM_PRESENT)
        {
            as->page_dir[i] = kernel_page_dir[i];
        }
    }

    as->regions = NULL;
    as->ref_count = 1;
    as->next = NULL;

    return as;
}

/* Destroy address space */
void vmm_destroy_as(struct vmm_address_space *as)
{
    if (!as)
        return;

    /* Free user mappings (first 768 entries) */
    for (uint32_t pd_idx = 0; pd_idx < 768; pd_idx++)
    {
        uint32_t pde = as->page_dir[pd_idx];
        if (pde & VMM_PRESENT)
        {
            uint32_t *pt = (uint32_t *)(pde & ~0xFFF);

            /* Free all pages */
            for (uint32_t pt_idx = 0; pt_idx < 1024; pt_idx++)
            {
                if (pt[pt_idx] & VMM_PRESENT)
                {
                    uint32_t phys = pt[pt_idx] & ~0xFFF;
                    if (phys >= 0x100000)
                    {
                        pmm_free_block((void *)phys);
                    }
                }
            }

            /* Free page table */
            pmm_free_block(pt);
        }
    }

    /* Free page directory */
    pmm_free_block(as->page_dir);

    /* Free regions */
    struct vmm_region *region = as->regions;
    while (region)
    {
        struct vmm_region *next = region->next;
        kfree(region);
        region = next;
    }

    kfree(as);
}

/* Switch address space */
void vmm_switch_as(struct vmm_address_space *as)
{
    if (!as || !as->page_dir)
        return;

    vmm_current_as = as;
    asm volatile("mov %0, %%cr3" ::"r"(as->page_dir) : "memory");
}