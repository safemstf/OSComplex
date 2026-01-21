# user/Makefile - Build User Mode Programs

# Toolchain
CC = gcc -m32
LD = ld -m elf_i386
OBJCOPY = objcopy

# Flags for user programs
CFLAGS = -std=c99 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector -Wall -Wextra

# User programs to build
PROGRAMS = hello usertest_fork

all: $(PROGRAMS)

# Build hello program
hello: hello.o
	@echo "[LD] Linking hello..."
	@$(LD) -T user.ld hello.o -o hello.elf
	@$(OBJCOPY) -O binary hello.elf hello.bin
	@echo "[✓] User program 'hello' built successfully"
	@ls -lh hello.elf hello.bin

hello.o: hello.c
	@echo "[CC] Compiling hello.c..."
	@$(CC) $(CFLAGS) -c hello.c -o hello.o

# Build usertest_fork program
usertest_fork: usertest_fork.o
	@echo "[LD] Linking usertest_fork..."
	@$(LD) -T user.ld usertest_fork.o -o usertest_fork.elf
	@$(OBJCOPY) -O binary usertest_fork.elf usertest_fork.bin
	@echo "[✓] User program 'usertest_fork' built successfully"
	@ls -lh usertest_fork.elf usertest_fork.bin

usertest_fork.o: usertest_fork.c
	@echo "[CC] Compiling usertest_fork.c..."
	@$(CC) $(CFLAGS) -c usertest_fork.c -o usertest_fork.o

clean:
	@rm -f *.o *.elf *.bin
	@echo "[✓] User programs cleaned"

.PHONY: all clean