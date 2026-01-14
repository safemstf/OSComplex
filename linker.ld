ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    /* Physical address where bootloader loads us */
    . = 0x100000;

    /* Keep track of start of kernel */
    kernel_start = .;

    /* Multiboot header and boot code MUST come first */
    .boot :
    {
        *(.multiboot)
        *(.multiboot.*)
    }

    /* Code - immediately after multiboot header */
    .text ALIGN(4) :
    {
        *(.text)
        *(.text.*)
    }

    /* Read-only data */
    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Initialized data */
    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    }

    /* Uninitialized data */
    .bss ALIGN(4096) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* Mark end of kernel */
    kernel_end = .;
}
