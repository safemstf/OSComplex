/* user/user.ld - Linker Script for User Programs
 * 
 * This tells the linker how to organize the program in memory.
 * User programs are loaded at 0x08048000 (standard Linux convention).
 */

ENTRY(_start)

SECTIONS
{
    /* Code starts at 0x08048000 (128MB + 288KB)
     * This is the standard Linux user program base address */
    . = 0x08048000;
    
    .text : {
        *(.text)
        *(.text.*)
    }
    
    .rodata ALIGN(4096) : {
        *(.rodata)
        *(.rodata.*)
    }
    
    .data ALIGN(4096) : {
        *(.data)
        *(.data.*)
    }
    
    .bss ALIGN(4096) : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
}
