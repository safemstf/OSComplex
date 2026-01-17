# Makefile for OSComplex - AI-Native Operating System

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
KERNEL = OSComplex.bin

# ============================================================
# SOURCE FILES (organized by directory)
# ============================================================
BOOT_ASM = boot/boot.s
INT_ASM = interrupts/interrupts.s interrupts/syscall.s
INT_C = interrupts/idt.c interrupts/isr.c interrupts/pagefault.c
DRIVER_C = drivers/terminal.c drivers/keyboard.c drivers/pic.c drivers/timer.c
KERNEL_C = kernel/kernel.c kernel/fpu.c kernel/task.c kernel/scheduler.c kernel/syscall.c
LIB_C = lib/string.c
AI_C = ai/ai.c
SHELL_C = shell/shell.c shell/test_tasks.c
MM_C = mm/pmm.c mm/paging.c mm/heap.c mm/vmm.c

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
MM_OBJ = $(MM_C:.c=.o)

OBJS = $(BOOT_OBJ) $(INT_OBJ) $(DRIVER_OBJ) $(KERNEL_OBJ) $(LIB_OBJ) $(AI_OBJ) $(SHELL_OBJ) $(MM_OBJ)

# ============================================================
# BUILD TARGETS
# ============================================================
.PHONY: all clean run debug

all: $(KERNEL)
	@echo ""
	@echo "╔════════════════════════════════════════╗"
	@echo "║   OSComplex Build Complete! 🚀     ║"
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
	@echo "║     Launching OSComplex! 🚀        ║"
	@echo "╚════════════════════════════════════════╝"
	@echo ""
	@echo "Press Ctrl+Alt+G to release mouse"
	@echo "Press Ctrl+C to exit"
	@echo ""
	qemu-system-i386 -kernel $(KERNEL).elf -m 32M

debug: $(KERNEL)
	@echo ""
	@echo "Launching in debug mode..."
	@echo "Connect with: gdb -ex 'target remote localhost:1234'"
	@echo ""
	qemu-system-i386 -kernel $(KERNEL).elf -s -S -m 32M

# ============================================================
# UTILITY TARGETS
# ============================================================

clean:
	@echo "Cleaning build artifacts..."
	@rm -f $(OBJS) $(KERNEL) $(KERNEL).elf
	@echo "[✓] Clean complete"

info:
	@echo "=== OSComplex Build Configuration ==="
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