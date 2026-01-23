# user/Makefile - Build User Mode Programs

# Toolchain
CC = gcc -m32
LD = ld -m elf_i386
OBJCOPY = objcopy

# Flags for user programs
CFLAGS = -std=c99 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector -Wall -Wextra

# User programs to build
PROGRAMS = hello usertest_fork assignment_demo assignment_demo_real ls

all: $(PROGRAMS)

# Build hello program
hello: hello.o
	@echo "[LD] Linking hello..."
	@$(LD) -T user.ld hello.o -o hello.elf
	@$(OBJCOPY) -O binary hello.elf hello.bin
	@echo "[✓] User program 'hello' built successfully"

hello.o: hello.c
	@echo "[CC] Compiling hello.c..."
	@$(CC) $(CFLAGS) -c hello.c -o hello.o

# Build usertest_fork program
usertest_fork: usertest_fork.o
	@echo "[LD] Linking usertest_fork..."
	@$(LD) -T user.ld usertest_fork.o -o usertest_fork.elf
	@$(OBJCOPY) -O binary usertest_fork.elf usertest_fork.bin
	@echo "[✓] User program 'usertest_fork' built successfully"

usertest_fork.o: usertest_fork.c
	@echo "[CC] Compiling usertest_fork.c..."
	@$(CC) $(CFLAGS) -c usertest_fork.c -o usertest_fork.o

# Build assignment_demo program
assignment_demo: assignment_demo.o
	@echo "[LD] Linking assignment_demo..."
	@$(LD) -T user.ld assignment_demo.o -o assignment_demo.elf
	@$(OBJCOPY) -O binary assignment_demo.elf assignment_demo.bin
	@echo "[✓] User program 'assignment_demo' built successfully"

assignment_demo.o: assignment_demo.c
	@echo "[CC] Compiling assignment_demo.c..."
	@$(CC) $(CFLAGS) -c assignment_demo.c -o assignment_demo.o

# Build assignment_demo_real program
assignment_demo_real: assignment_demo_real.o
	@echo "[LD] Linking assignment_demo_real..."
	@$(LD) -T user.ld assignment_demo_real.o -o assignment_demo_real.elf
	@$(OBJCOPY) -O binary assignment_demo_real.elf assignment_demo_real.bin
	@echo "[✓] User program 'assignment_demo_real' built successfully"

assignment_demo_real.o: assignment_demo_real.c
	@echo "[CC] Compiling assignment_demo_real.c..."
	@$(CC) $(CFLAGS) -c assignment_demo_real.c -o assignment_demo_real.o

# Build ls program
ls: ls.o
	@echo "[LD] Linking ls..."
	@$(LD) -T user.ld ls.o -o ls.elf
	@$(OBJCOPY) -O binary ls.elf ls.bin
	@echo "[✓] User program 'ls' built successfully"

ls.o: ls.c
	@echo "[CC] Compiling ls.c..."
	@$(CC) $(CFLAGS) -c ls.c -o ls.o

clean:
	@rm -f *.o *.elf *.bin
	@echo "[✓] User programs cleaned"

.PHONY: all clean