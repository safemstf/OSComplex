ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

PHDRS
{
    text PT_LOAD FLAGS(5); /* R + X */
    data PT_LOAD FLAGS(6); /* R + W */
}

SECTIONS
{
    . = 0x100000;

    kernel_start = .;

    .boot :
    {
        *(.multiboot)
        *(.multiboot.*)
    } :text

    .text ALIGN(4) :
    {
        *(.text)
        *(.text.*)
    } :text

    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
    } :text

    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    } :data

    .bss ALIGN(4096) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :data

    kernel_end = .;
}
