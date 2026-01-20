/* kernel/elf.c - ELF Binary Loader Implementation - FIXED
 * 
 * CRITICAL FIX: Don't switch CR3 during loading!
 * Instead, map pages into the task's page directory while staying in kernel space.
 */

#include "elf.h"
#include "kernel.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"

/* Validate ELF header */
int elf_validate(void *elf_data)
{
    elf32_ehdr_t *ehdr = (elf32_ehdr_t *)elf_data;
    
    /* Check magic number */
    if (ehdr->e_ident[0] != 0x7F ||
        ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' ||
        ehdr->e_ident[3] != 'F') {
        return 0;  /* Not an ELF file */
    }
    
    /* Check 32-bit */
    if (ehdr->e_ident[4] != ELFCLASS32) {
        terminal_writestring("[ELF] Error: Not a 32-bit ELF\n");
        return 0;
    }
    
    /* Check little endian */
    if (ehdr->e_ident[5] != ELFDATA2LSB) {
        terminal_writestring("[ELF] Error: Not little endian\n");
        return 0;
    }
    
    /* Check executable */
    if (ehdr->e_type != ET_EXEC) {
        terminal_writestring("[ELF] Error: Not an executable\n");
        return 0;
    }
    
    /* Check i386 */
    if (ehdr->e_machine != EM_386) {
        terminal_writestring("[ELF] Error: Not for i386\n");
        return 0;
    }
    
    return 1;  /* Valid ELF */
}

/* Get entry point from ELF */
uint32_t elf_get_entry(void *elf_data)
{
    elf32_ehdr_t *ehdr = (elf32_ehdr_t *)elf_data;
    return ehdr->e_entry;
}

/* Helper: Map a page in the task's address space and get kernel-accessible address
 * 
 * This is the KEY to avoiding the CR3 switch problem!
 * We allocate a physical page, map it in BOTH the task's page directory
 * AND temporarily in kernel space so we can write to it.
 */
static void* map_user_page_accessible(task_t *task, uint32_t user_vaddr, uint32_t flags) {
    /* Allocate physical page */
    void *phys = pmm_alloc_block();
    if (!phys) {
        return NULL;
    }
    
    /* Map in task's page directory (for when task runs) */
    /* We need to manually manipulate the task's page directory here */
    uint32_t pd_idx = (user_vaddr >> 22) & 0x3FF;
    uint32_t pt_idx = (user_vaddr >> 12) & 0x3FF;
    
    uint32_t *task_pd = task->page_directory;
    
    /* Get or create page table in task's address space */
    if (!(task_pd[pd_idx] & VMM_PRESENT)) {
        /* Allocate page table */
        void *pt_phys = pmm_alloc_block();
        if (!pt_phys) {
            pmm_free_block(phys);
            return NULL;
        }
        
        /* Zero it (we need to map it temporarily to zero it) */
        uint32_t temp_vaddr = 0xF0000000;  /* Temporary kernel mapping area */
        vmm_map_page(temp_vaddr, (uint32_t)pt_phys, VMM_PRESENT | VMM_WRITE);
        memset((void*)temp_vaddr, 0, PAGE_SIZE);
        vmm_unmap_page(temp_vaddr);
        
        /* Install in task's page directory */
        task_pd[pd_idx] = ((uint32_t)pt_phys) | VMM_PRESENT | VMM_WRITE | VMM_USER;
    }
    
    /* Get page table */
    uint32_t *task_pt = (uint32_t*)(task_pd[pd_idx] & ~0xFFF);
    
    /* Map page in task's page table */
    task_pt[pt_idx] = ((uint32_t)phys) | (flags & 0xFFF);
    
    /* Also map in kernel space temporarily so we can write to it */
    uint32_t kernel_temp_vaddr = 0xE0000000 + ((uint32_t)phys & 0x00FFFFFF);
    vmm_map_page(kernel_temp_vaddr, (uint32_t)phys, VMM_PRESENT | VMM_WRITE);
    
    return (void*)kernel_temp_vaddr;
}

/* Load ELF into task's address space - FIXED VERSION */
int elf_load(task_t *task, void *elf_data)
{
    /* Validate ELF */
    if (!elf_validate(elf_data)) {
        return 0;
    }
    
    elf32_ehdr_t *ehdr = (elf32_ehdr_t *)elf_data;
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[ELF] Loading executable...\n");
    terminal_writestring("[ELF] Entry point: 0x");
    terminal_write_hex(ehdr->e_entry);
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    /* Get program headers */
    elf32_phdr_t *phdr = (elf32_phdr_t *)((uint8_t *)elf_data + ehdr->e_phoff);
    
    /* Load each LOAD segment - WITHOUT SWITCHING CR3! */
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) {
            continue;  /* Skip non-loadable segments */
        }
        
        terminal_writestring("[ELF] Loading segment ");
        terminal_write_dec(i);
        terminal_writestring(": vaddr=0x");
        terminal_write_hex(phdr[i].p_vaddr);
        terminal_writestring(" filesz=");
        terminal_write_dec(phdr[i].p_filesz);
        terminal_writestring(" memsz=");
        terminal_write_dec(phdr[i].p_memsz);
        terminal_writestring(" flags=");
        if (phdr[i].p_flags & PF_R) terminal_putchar('R');
        if (phdr[i].p_flags & PF_W) terminal_putchar('W');
        if (phdr[i].p_flags & PF_X) terminal_putchar('X');
        terminal_writestring("\n");
        
        /* Calculate pages needed */
        uint32_t vaddr_start = phdr[i].p_vaddr & ~0xFFF;
        uint32_t vaddr_end = (phdr[i].p_vaddr + phdr[i].p_memsz + 0xFFF) & ~0xFFF;
        uint32_t num_pages = (vaddr_end - vaddr_start) / PAGE_SIZE;
        
        terminal_writestring("[ELF]   Allocating ");
        terminal_write_dec(num_pages);
        terminal_writestring(" pages starting at 0x");
        terminal_write_hex(vaddr_start);
        terminal_writestring("\n");
        
        /* Map pages and copy data */
        for (uint32_t page_num = 0; page_num < num_pages; page_num++) {
            uint32_t user_vaddr = vaddr_start + page_num * PAGE_SIZE;
            
            /* Determine flags */
            uint32_t flags = VMM_PRESENT | VMM_USER;
            if (phdr[i].p_flags & PF_W) {
                flags |= VMM_WRITE;
            }
            
            /* Map page (accessible from kernel) */
            void *kernel_addr = map_user_page_accessible(task, user_vaddr, flags);
            if (!kernel_addr) {
                terminal_writestring("[ELF] ERROR: Out of memory\n");
                return 0;
            }
            
            /* Zero the entire page first */
            memset(kernel_addr, 0, PAGE_SIZE);
            
            /* Copy data if this page contains file data */
            uint32_t page_offset_in_segment = user_vaddr - phdr[i].p_vaddr;
            
            if (page_offset_in_segment < phdr[i].p_filesz) {
                /* This page has data to copy */
                uint32_t copy_start = (page_offset_in_segment > 0) ? 0 : (phdr[i].p_vaddr & 0xFFF);
                uint32_t file_offset = phdr[i].p_offset + page_offset_in_segment;
                uint32_t copy_size = PAGE_SIZE - copy_start;
                
                /* Don't copy past end of file data */
                uint32_t remaining = phdr[i].p_filesz - page_offset_in_segment;
                if (copy_size > remaining) {
                    copy_size = remaining;
                }
                
                if (copy_size > 0) {
                    uint8_t *src = (uint8_t *)elf_data + file_offset;
                    uint8_t *dst = (uint8_t *)kernel_addr + copy_start;
                    memcpy(dst, src, copy_size);
                }
            }
            
            /* Unmap from kernel temp space */
            vmm_unmap_page((uint32_t)kernel_addr);
        }
        
        /* Track memory regions */
        if (phdr[i].p_flags & PF_X) {
            task->code_start = phdr[i].p_vaddr;
            task->code_end = phdr[i].p_vaddr + phdr[i].p_memsz;
        } else if (phdr[i].p_flags & PF_W) {
            task->data_start = phdr[i].p_vaddr;
            task->data_end = phdr[i].p_vaddr + phdr[i].p_memsz;
        }
    }
    
    /* Set entry point - this is used by task_setup_user_context */
    task->code_start = ehdr->e_entry;
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[ELF] ✓ Executable loaded successfully\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    return 1;
}