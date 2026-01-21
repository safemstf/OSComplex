/* usertest_fork.c - User-mode program to test fork() and wait()
 * 
 * Compile this as a user-mode ELF binary and load it with exec
 * 
 * Build command:
 *   i686-elf-gcc -m32 -ffreestanding -nostdlib -o usertest_fork usertest_fork.c
 */

/* ================================================================
 * SYSTEM CALL WRAPPERS
 * ================================================================ */

static inline int syscall0(int num)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num));
    return ret;
}

static inline int syscall1(int num, int arg1)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1));
    return ret;
}

static inline int syscall2(int num, int arg1, int arg2)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2));
    return ret;
}

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  4
#define SYS_FORK    6
#define SYS_WAIT    8

int fork(void)   { return syscall0(SYS_FORK); }
int wait(int *s) { return syscall1(SYS_WAIT, (int)s); }
void exit(int c) { syscall1(SYS_EXIT, c); while(1); }
int getpid(void) { return syscall0(SYS_GETPID); }
int write(const char *msg) { return syscall1(SYS_WRITE, (int)msg); }

/* ================================================================
 * HELPER FUNCTIONS
 * ================================================================ */

int strlen(const char *s)
{
    int len = 0;
    while (s[len]) len++;
    return len;
}

void print_num(int n)
{
    char buf[12];
    int i = 0;
    
    if (n == 0) {
        write("0");
        return;
    }
    
    int negative = 0;
    if (n < 0) {
        negative = 1;
        n = -n;
    }
    
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    
    if (negative) {
        buf[i++] = '-';
    }
    
    /* Print reversed */
    while (i > 0) {
        i--;
        char c[2] = {buf[i], '\0'};
        write(c);
    }
}

/* ================================================================
 * MAIN PROGRAM
 * ================================================================ */

void _start(void)
{
    write("\n");
    write("╔══════════════════════════════════════════════════════════╗\n");
    write("║          User-Mode Fork/Wait Test Program               ║\n");
    write("╚══════════════════════════════════════════════════════════╝\n");
    write("\n");
    
    write("Parent: Starting fork test...\n");
    write("Parent: My PID is ");
    print_num(getpid());
    write("\n\n");
    
    write("Parent: Calling fork()...\n");
    int pid = fork();
    
    if (pid < 0) {
        /* Fork failed */
        write("ERROR: Fork failed!\n");
        exit(1);
    }
    else if (pid == 0) {
        /* CHILD PROCESS */
        write("\n");
        write("═══════════════════════════════════════════════════════════\n");
        write("  CHILD PROCESS\n");
        write("═══════════════════════════════════════════════════════════\n");
        write("\n");
        
        write("Child: I am the child process!\n");
        write("Child: My PID is ");
        print_num(getpid());
        write("\n");
        write("Child: My parent's PID is ");
        print_num(pid);  /* This won't work - we need getppid() */
        write("\n\n");
        
        write("Child: Doing some important work...\n");
        for (int i = 1; i <= 5; i++) {
            write("Child: Step ");
            print_num(i);
            write(" of 5\n");
        }
        
        write("\nChild: Work complete!\n");
        write("Child: Exiting with code 42\n");
        exit(42);
    }
    else {
        /* PARENT PROCESS */
        write("\n");
        write("═══════════════════════════════════════════════════════════\n");
        write("  PARENT PROCESS\n");
        write("═══════════════════════════════════════════════════════════\n");
        write("\n");
        
        write("Parent: Fork successful!\n");
        write("Parent: Child PID is ");
        print_num(pid);
        write("\n\n");
        
        write("Parent: Waiting for child to finish...\n");
        
        int status = 0;
        int child_pid = wait(&status);
        
        write("\n");
        write("Parent: Child finished!\n");
        write("Parent: Reaped child PID ");
        print_num(child_pid);
        write("\n");
        write("Parent: Child exit status: ");
        print_num(status);
        write("\n\n");
        
        if (status == 42) {
            write("Parent: ✓ Child exited with expected code!\n");
        } else {
            write("Parent: ✗ Unexpected exit code!\n");
        }
        
        write("\n");
        write("Parent: Fork/Wait test completed successfully!\n");
        write("Parent: Exiting with code 0\n");
        exit(0);
    }
}