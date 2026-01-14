#!/bin/bash
# Comprehensive Multiboot Fix Script

echo "╔══════════════════════════════════════════════════════════╗"
echo "║  LinuxComplex - Comprehensive Multiboot Fix v2          ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

# Backup
echo "[1/5] Backing up files..."
cp linker.ld linker.ld.backup 2>/dev/null
cp Makefile Makefile.backup 2>/dev/null
echo "  ✓ Backups created"
echo ""

# Fix 1: Update Makefile to add -n flag to linker
echo "[2/5] Updating Makefile..."
cat > Makefile << 'MAKEFILE_END'
# Makefile for LinuxComplex - AI-Native Operating System

# ============================================================
# TOOLCHAIN CONFIGURATION
# ============================================================
AS = as --32
CC = gcc -m32
LD = ld -m elf_i386
OBJCOPY = objcopy

# ============================================================
# COMPILER FLAGS
# ============================================================
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Ikernel

# ============================================================
# LINKER FLAGS
# ============================================================
# -n = don't page-align data, keeps sections contiguous
# -T = use our linker script
LDFLAGS = -n -T linker.ld -nostdlib

# ============================================================
# OUTPUT FILES
# ============================================================
KERNEL = linuxcomplex.bin

# ============================================================
# SOURCE FILES (organized by directory)
# ============================================================
BOOT_ASM = boot/boot.s
INT_ASM = interrupts/interrupts.s
INT_C = interrupts/idt.c interrupts/isr.c
DRIVER_C = drivers/terminal.c drivers/keyboard.c drivers/pic.c
KERNEL_C = kernel/kernel.c
LIB_C = lib/string.c
AI_C = ai/ai.c
SHELL_C = shell/shell.c

# ============================================================
# OBJECT FILES
# ============================================================
BOOT_OBJ = boot/boot.o
INT_OBJ = $(INT_ASM:.s=.o) $(INT_C:.c=.o)
DRIVER_OBJ = $(DRIVER_C:.c=.o)
KERNEL_OBJ = $(KERNEL_C:.c=.o)
LIB_OBJ = $(LIB_C:.c=.o)
AI_OBJ = $(AI_C:.c=.o)
SHELL_OBJ = $(SHELL_C:.c=.o)

OBJS = $(BOOT_OBJ) $(INT_OBJ) $(DRIVER_OBJ) $(KERNEL_OBJ) $(LIB_OBJ) $(AI_OBJ) $(SHELL_OBJ)

# ============================================================
# BUILD TARGETS
# ============================================================
.PHONY: all clean run debug

all: $(KERNEL)
	@echo ""
	@echo "╔════════════════════════════════════════╗"
	@echo "║   LinuxComplex Build Complete! 🚀     ║"
	@echo "╚════════════════════════════════════════╝"
	@echo ""
	@echo "Run with: make run"

# ============================================================
# COMPILATION RULES
# ============================================================

%.o: %.s
	@echo "[AS] $<"
	@$(AS) $< -o $@

%.o: %.c kernel/kernel.h
	@echo "[CC] $<"
	@$(CC) -c $< -o $@ $(CFLAGS)

# ============================================================
# LINKING
# ============================================================
$(KERNEL).elf: $(OBJS) linker.ld
	@echo ""
	@echo "[LD] Linking kernel ELF..."
	@$(LD) $(LDFLAGS) $(OBJS) -o $(KERNEL).elf

$(KERNEL): $(KERNEL).elf
	@echo "[OBJCOPY] Converting to flat binary..."
	@$(OBJCOPY) -O binary $(KERNEL).elf $(KERNEL)
	@echo "[✓] Kernel binary created: $(KERNEL)"

# ============================================================
# RUN TARGETS
# ============================================================

run: $(KERNEL)
	@echo ""
	@echo "╔════════════════════════════════════════╗"
	@echo "║     Launching LinuxComplex! 🚀        ║"
	@echo "╚════════════════════════════════════════╝"
	@echo ""
	@echo "Press Ctrl+Alt+G to release mouse"
	@echo "Press Ctrl+C to exit"
	@echo ""
	qemu-system-i386 -kernel $(KERNEL) -m 32M

debug: $(KERNEL)
	@echo ""
	@echo "Launching in debug mode..."
	@echo "Connect with: gdb -ex 'target remote localhost:1234'"
	@echo ""
	qemu-system-i386 -kernel $(KERNEL) -s -S -m 32M

# ============================================================
# UTILITY TARGETS
# ============================================================

clean:
	@echo "Cleaning build artifacts..."
	@rm -f $(OBJS) $(KERNEL) $(KERNEL).elf
	@echo "[✓] Clean complete"

info:
	@echo "=== LinuxComplex Build Configuration ==="
	@echo "Compiler: $(CC)"
	@echo "Assembler: $(AS)"
	@echo "Linker: $(LD)"
	@echo "Flags: $(CFLAGS)"
	@echo ""
	@echo "Source files:"
	@echo "  Boot: $(BOOT_ASM)"
	@echo "  Interrupts: $(INT_ASM) $(INT_C)"
	@echo "  Drivers: $(DRIVER_C)"
	@echo "  Kernel: $(KERNEL_C)"
	@echo "  Libraries: $(LIB_C)"
	@echo "  AI: $(AI_C)"
	@echo "  Shell: $(SHELL_C)"
	@echo ""
	@echo "Output: $(KERNEL)"
MAKEFILE_END

echo "  ✓ Makefile updated with -n flag"
echo ""

# Fix 2: Install OSDev-style linker script
echo "[3/5] Installing OSDev-style linker script..."
cat > linker.ld << 'LINKER_END'
/* linker.ld - Battle-tested OSDev.org style */

ENTRY(_start)

SECTIONS
{
    . = 1M;

    .boot :
    {
        *(.multiboot)
    }

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
LINKER_END

echo "  ✓ Linker script installed"
echo ""

# Fix 3: Clean and rebuild
echo "[4/5] Clean and rebuild..."
make clean
make
echo ""

# Fix 4: Verify
echo "[5/5] Verification..."
if [ -f "linuxcomplex.bin" ]; then
    echo "First 16 bytes:"
    xxd -l 16 linuxcomplex.bin || od -A x -t x1 -N 16 linuxcomplex.bin
    echo ""
    
    if xxd -l 4 linuxcomplex.bin 2>/dev/null | grep -q "02b0 ad1b"; then
        echo "╔══════════════════════════════════════════════════════════╗"
        echo "║              ✓✓✓ SUCCESS! ✓✓✓                           ║"
        echo "║         Multiboot magic found at offset 0!               ║"
        echo "╚══════════════════════════════════════════════════════════╝"
        echo ""
        echo "Your kernel should boot now! Run: make run"
    else
        echo "[!] Still not working. Checking ELF sections..."
        readelf -S linuxcomplex.bin.elf | grep -E "multiboot|text"
        echo ""
        echo "If .text has a lower address than .multiboot, we have a deeper issue."
        echo "Try: qemu-system-x86_64 -kernel linuxcomplex.bin.elf"
        echo "(boot the ELF directly instead of the binary)"
    fi
else
    echo "[!] Build failed"
fi