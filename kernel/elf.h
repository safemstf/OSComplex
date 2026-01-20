/* kernel/elf.h - ELF Binary Format Support
 * 
 * Implements loading of ELF (Executable and Linkable Format) binaries.
 * This is the standard executable format for Linux and most Unix systems.
 */

#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include "task.h"

/* ELF magic number */
#define ELF_MAGIC 0x464C457F  /* 0x7F, 'E', 'L', 'F' */

/* ELF classes */
#define ELFCLASS32 1  /* 32-bit */
#define ELFCLASS64 2  /* 64-bit */

/* ELF data encoding */
#define ELFDATA2LSB 1  /* Little endian */
#define ELFDATA2MSB 2  /* Big endian */

/* ELF types */
#define ET_NONE 0  /* No file type */
#define ET_REL  1  /* Relocatable */
#define ET_EXEC 2  /* Executable */
#define ET_DYN  3  /* Shared object */
#define ET_CORE 4  /* Core file */

/* ELF machine types */
#define EM_386  3  /* Intel 80386 */

/* Program header types */
#define PT_NULL    0  /* Unused entry */
#define PT_LOAD    1  /* Loadable segment */
#define PT_DYNAMIC 2  /* Dynamic linking info */
#define PT_INTERP  3  /* Interpreter path */
#define PT_NOTE    4  /* Auxiliary info */

/* Program header flags */
#define PF_X 0x1  /* Execute */
#define PF_W 0x2  /* Write */
#define PF_R 0x4  /* Read */

/* Section header types */
#define SHT_NULL     0  /* Unused */
#define SHT_PROGBITS 1  /* Program data */
#define SHT_SYMTAB   2  /* Symbol table */
#define SHT_STRTAB   3  /* String table */
#define SHT_RELA     4  /* Relocation with addends */
#define SHT_NOBITS   8  /* BSS (no data in file) */

/* ================================================================
 * ELF Header (32-bit)
 * ================================================================ */
typedef struct {
    uint8_t  e_ident[16];     /* Magic number and other info */
    uint16_t e_type;          /* Object file type */
    uint16_t e_machine;       /* Architecture */
    uint32_t e_version;       /* Object file version */
    uint32_t e_entry;         /* Entry point virtual address */
    uint32_t e_phoff;         /* Program header table file offset */
    uint32_t e_shoff;         /* Section header table file offset */
    uint32_t e_flags;         /* Processor-specific flags */
    uint16_t e_ehsize;        /* ELF header size in bytes */
    uint16_t e_phentsize;     /* Program header table entry size */
    uint16_t e_phnum;         /* Program header table entry count */
    uint16_t e_shentsize;     /* Section header table entry size */
    uint16_t e_shnum;         /* Section header table entry count */
    uint16_t e_shstrndx;      /* Section header string table index */
} __attribute__((packed)) elf32_ehdr_t;

/* ================================================================
 * Program Header (32-bit)
 * ================================================================ */
typedef struct {
    uint32_t p_type;    /* Segment type */
    uint32_t p_offset;  /* Segment file offset */
    uint32_t p_vaddr;   /* Segment virtual address */
    uint32_t p_paddr;   /* Segment physical address */
    uint32_t p_filesz;  /* Segment size in file */
    uint32_t p_memsz;   /* Segment size in memory */
    uint32_t p_flags;   /* Segment flags */
    uint32_t p_align;   /* Segment alignment */
} __attribute__((packed)) elf32_phdr_t;

/* ================================================================
 * Section Header (32-bit)
 * ================================================================ */
typedef struct {
    uint32_t sh_name;       /* Section name (string tbl index) */
    uint32_t sh_type;       /* Section type */
    uint32_t sh_flags;      /* Section flags */
    uint32_t sh_addr;       /* Section virtual addr at execution */
    uint32_t sh_offset;     /* Section file offset */
    uint32_t sh_size;       /* Section size in bytes */
    uint32_t sh_link;       /* Link to another section */
    uint32_t sh_info;       /* Additional section information */
    uint32_t sh_addralign;  /* Section alignment */
    uint32_t sh_entsize;    /* Entry size if section holds table */
} __attribute__((packed)) elf32_shdr_t;

/* ================================================================
 * ELF Loader Functions
 * ================================================================ */

/* Validate ELF header */
int elf_validate(void *elf_data);

/* Load ELF into task's address space */
int elf_load(task_t *task, void *elf_data);

/* Get entry point from ELF */
uint32_t elf_get_entry(void *elf_data);

#endif /* ELF_H */