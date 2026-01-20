/* kernel/elf.c - ELF Binary Loader Implementation */

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

/* Load ELF into task's address space */
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
    
    /* Get program headers */
    elf32_phdr_t *phdr = (elf32_phdr_t *)((uint8_t *)elf_data + ehdr->e_phoff);
    
    /* Switch to task's address space for loading */
    uint32_t *old_pd = vmm_current_as->page_dir;
    vmm_current_as->page_dir = task->page_directory;
    __asm__ volatile("mov %0, %%cr3" :: "r"(task->page_directory) : "memory");
    
    /* Load each LOAD segment */
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) {
            continue;  /* Skip non-loadable segments */
        }
        
        terminal_writestring("[ELF] Loading segment ");
        terminal_write_dec(i);
        terminal_writestring(": 0x");
        terminal_write_hex(phdr[i].p_vaddr);
        terminal_writestring(" - 0x");
        terminal_write_hex(phdr[i].p_vaddr + phdr[i].p_memsz);
        terminal_writestring(" (");
        
        if (phdr[i].p_flags & PF_R) terminal_putchar('R');
        if (phdr[i].p_flags & PF_W) terminal_putchar('W');
        if (phdr[i].p_flags & PF_X) terminal_putchar('X');
        terminal_writestring(")\n");
        
        /* Calculate number of pages needed */
        uint32_t vaddr_start = phdr[i].p_vaddr & ~0xFFF;  /* Page align down */
        uint32_t vaddr_end = (phdr[i].p_vaddr + phdr[i].p_memsz + 0xFFF) & ~0xFFF;
        uint32_t num_pages = (vaddr_end - vaddr_start) / PAGE_SIZE;
        
        /* Allocate and map pages */
        for (uint32_t j = 0; j < num_pages; j++) {
            uint32_t vaddr = vaddr_start + j * PAGE_SIZE;
            void *phys = pmm_alloc_block();
            
            if (!phys) {
                terminal_writestring("[ELF] Error: Out of memory\n");
                vmm_current_as->page_dir = old_pd;
                __asm__ volatile("mov %0, %%cr3" :: "r"(old_pd) : "memory");
                return 0;
            }
            
            /* Map with appropriate flags */
            uint32_t flags = VMM_PRESENT | VMM_USER;
            if (phdr[i].p_flags & PF_W) {
                flags |= VMM_WRITE;
            }
            
            vmm_map_page(vaddr, (uint32_t)phys, flags);
            
            /* Zero the page */
            memset((void *)vaddr, 0, PAGE_SIZE);
        }
        
        /* Copy segment data from file */
        if (phdr[i].p_filesz > 0) {
            uint8_t *src = (uint8_t *)elf_data + phdr[i].p_offset;
            uint8_t *dst = (uint8_t *)phdr[i].p_vaddr;
            memcpy(dst, src, phdr[i].p_filesz);
        }
        
        /* Zero BSS (p_memsz > p_filesz) */
        if (phdr[i].p_memsz > phdr[i].p_filesz) {
            uint8_t *bss_start = (uint8_t *)(phdr[i].p_vaddr + phdr[i].p_filesz);
            size_t bss_size = phdr[i].p_memsz - phdr[i].p_filesz;
            memset(bss_start, 0, bss_size);
        }
        
        /* Track memory regions */
        if (phdr[i].p_flags & PF_X) {
            /* Code segment */
            task->code_start = phdr[i].p_vaddr;
            task->code_end = phdr[i].p_vaddr + phdr[i].p_memsz;
        } else if (phdr[i].p_flags & PF_W) {
            /* Data segment */
            task->data_start = phdr[i].p_vaddr;
            task->data_end = phdr[i].p_vaddr + phdr[i].p_memsz;
        }
    }
    
    /* Restore original address space */
    vmm_current_as->page_dir = old_pd;
    __asm__ volatile("mov %0, %%cr3" :: "r"(old_pd) : "memory");
    
    /* Set entry point */
    task->context.eip = ehdr->e_entry;
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[ELF] ✓ Executable loaded successfully\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    return 1;
}