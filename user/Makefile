# user/Makefile - Build User Mode Programs

# Toolchain
CC = gcc -m32
LD = ld -m elf_i386
OBJCOPY = objcopy

# Flags for user programs
CFLAGS = -std=c99 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
         -Wall -Wextra -O2

# User programs to build (short names for FAT16 compatibility)
PROGRAMS = hello counter sysinfo spin test

.PHONY: all clean

all: $(PROGRAMS)
	@echo ""
	@echo "========================================="
	@echo "  User programs built successfully!"
	@echo "========================================="
	@echo ""
	@echo "Available programs:"
	@echo "  - hello    : Simple hello world"
	@echo "  - counter  : Count 1-10 with delays"
	@echo "  - sysinfo  : Display system info"
	@echo "  - spin     : Spinner animation"
	@echo "  - test     : Basic syscall tests"
	@echo ""
	@echo "Run: ./install_user_programs.sh"
	@echo ""

# Generic rules
%.o: %.c ulib.h start.h
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Build hello
hello: hello.c ulib.h start.h
	@echo "[CC] hello.c"
	@$(CC) $(CFLAGS) -c hello.c -o hello.o
	@echo "[LD] hello.elf"
	@$(LD) -T user.ld hello.o -o hello.elf
	@$(OBJCOPY) -O binary hello.elf hello.bin
	@echo "[OK] hello"

# Build counter
counter: counter.c ulib.h start.h
	@echo "[CC] counter.c"
	@$(CC) $(CFLAGS) -c counter.c -o counter.o
	@echo "[LD] counter.elf"
	@$(LD) -T user.ld counter.o -o counter.elf
	@$(OBJCOPY) -O binary counter.elf counter.bin
	@echo "[OK] counter"

# Build sysinfo
sysinfo: sysinfo.c ulib.h start.h
	@echo "[CC] sysinfo.c"
	@$(CC) $(CFLAGS) -c sysinfo.c -o sysinfo.o
	@echo "[LD] sysinfo.elf"
	@$(LD) -T user.ld sysinfo.o -o sysinfo.elf
	@$(OBJCOPY) -O binary sysinfo.elf sysinfo.bin
	@echo "[OK] sysinfo"

# Build spin
spin: spin.c ulib.h start.h
	@echo "[CC] spin.c"
	@$(CC) $(CFLAGS) -c spin.c -o spin.o
	@echo "[LD] spin.elf"
	@$(LD) -T user.ld spin.o -o spin.elf
	@$(OBJCOPY) -O binary spin.elf spin.bin
	@echo "[OK] spin"

# Build test
test: test.c ulib.h start.h
	@echo "[CC] test.c"
	@$(CC) $(CFLAGS) -c test.c -o test.o
	@echo "[LD] test.elf"
	@$(LD) -T user.ld test.o -o test.elf
	@$(OBJCOPY) -O binary test.elf test.bin
	@echo "[OK] test"

clean:
	@rm -f *.o *.elf *.bin
	@echo "[OK] Cleaned user programs"
