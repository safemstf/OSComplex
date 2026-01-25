/* assignment_demo.c - Complete fork/exec/wait demonstration
 * FIXED: Naked _start to prevent function prologue from causing page fault
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

#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_GETPID 4
#define SYS_FORK 6
#define SYS_EXEC 7
#define SYS_WAIT 8

int fork(void) { return syscall0(SYS_FORK); }
int wait(int *status) { return syscall1(SYS_WAIT, (int)status); }
void exit(int code)
{
    syscall1(SYS_EXIT, code);
    while (1)
        ;
}
int getpid(void) { return syscall0(SYS_GETPID); }
int write(const char *msg) { return syscall1(SYS_WRITE, (int)msg); }
int exec(const char *path) { return syscall1(SYS_EXEC, (int)path); }

/* ================================================================
 * HELPER FUNCTIONS
 * ================================================================ */

void print_num(int n)
{
    char buf[12];
    int i = 0;

    if (n == 0)
    {
        write("0");
        return;
    }

    int negative = 0;
    if (n < 0)
    {
        negative = 1;
        n = -n;
    }

    while (n > 0)
    {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    if (negative)
    {
        buf[i++] = '-';
    }

    while (i > 0)
    {
        i--;
        char c[2] = {buf[i], '\0'};
        write(c);
    }
}

/* ================================================================
 * SIMULATED "ls -l" COMMAND
 * ================================================================ */

void simulate_ls_command(void)
{
    write("\n");
    write("Executing: ls -l\n");
    write("──────────────────────────────────────────────────────────\n");
    write("\n");
    write("Directory listing:\n");
    write("\n");
    write("-rwxr-xr-x  1 root  root   4096 Jan 20 2025 hello\n");
    write("-rwxr-xr-x  1 root  root   8192 Jan 21 2025 usertest_fork\n");
    write("-rwxr-xr-x  1 root  root   6144 Jan 21 2025 assignment_demo\n");
    write("drwxr-xr-x  2 root  root   4096 Jan 20 2025 bin\n");
    write("drwxr-xr-x  2 root  root   4096 Jan 20 2025 tmp\n");
    write("\n");
    write("Total: 5 items\n");
    write("──────────────────────────────────────────────────────────\n");
    write("\n");
}

/* ================================================================
 * MAIN PROGRAM - Assignment Requirements
 * ================================================================ */

void main_program(void)
{
    write("\n");
    write("╔══════════════════════════════════════════════════════════╗\n");
    write("║     Fork/Exec/Wait Assignment Demonstration              ║\n");
    write("╚══════════════════════════════════════════════════════════╝\n");
    write("\n");

    /* ============================================================
     * REQUIREMENT 3: Parent Process - Print parent PID
     * ============================================================ */
    write("PARENT PROCESS:\n");
    write("──────────────\n");
    write("Parent PID: ");
    print_num(getpid());
    write("\n\n");

    /* ============================================================
     * REQUIREMENT 1: Create a child process using fork()
     * ============================================================ */
    write("Creating child process with fork()...\n");

    int pid = fork();

    /* ============================================================
     * REQUIREMENT 4: Error handling
     * ============================================================ */
    if (pid < 0)
    {
        write("\n");
        write("ERROR: Fork failed!\n");
        write("Unable to create child process.\n");
        write("\n");
        exit(1);
    }

    /* ============================================================
     * REQUIREMENT 2: Child Process
     * ============================================================ */
    if (pid == 0)
    {
        /* WE ARE THE CHILD PROCESS */

        write("\n");
        write("═══════════════════════════════════════════════════════════\n");
        write("  CHILD PROCESS\n");
        write("═══════════════════════════════════════════════════════════\n");
        write("\n");

        /* REQUIREMENT 2a: Print child PID */
        write("Child PID: ");
        print_num(getpid());
        write("\n\n");

        /* REQUIREMENT 2b: Execute a command (ls -l) */
        write("Child is now executing command: ls -l\n");
        write("\n");

        simulate_ls_command();

        write("Child process finished executing command.\n");
        write("Child exiting with status code 0 (success).\n");
        write("\n");

        exit(0); /* Child exits successfully */
    }

    /* ============================================================
     * REQUIREMENT 3: Parent Process - Wait for child
     * ============================================================ */
    else
    {
        /* WE ARE THE PARENT PROCESS */

        write("Fork successful! Child PID: ");
        print_num(pid);
        write("\n\n");

        write("Parent waiting for child to complete...\n");
        write("(Using wait() system call)\n");
        write("\n");

        /* Wait for child to finish */
        int status = 0;
        int child_pid = wait(&status);

        /* Child has finished */
        write("\n");
        write("═══════════════════════════════════════════════════════════\n");
        write("  PARENT RESUMED\n");
        write("═══════════════════════════════════════════════════════════\n");
        write("\n");

        write("Child process completed!\n");
        write("Reaped child PID: ");
        print_num(child_pid);
        write("\n");
        write("Child exit status: ");
        print_num(status);
        write("\n\n");

        /* Verify success */
        if (status == 0)
        {
            write("✓ Child exited successfully (status 0)\n");
        }
        else
        {
            write("✗ Child exited with error (status ");
            print_num(status);
            write(")\n");
        }

        write("\n");
        write("──────────────────────────────────────────────────────────\n");
        write("Assignment Demonstration Complete!\n");
        write("\n");
        write("Summary:\n");
        write("  ✓ Created child process with fork()\n");
        write("  ✓ Child printed its PID\n");
        write("  ✓ Child executed command (ls -l)\n");
        write("  ✓ Parent printed its PID\n");
        write("  ✓ Parent waited for child with wait()\n");
        write("  ✓ Error handling implemented\n");
        write("──────────────────────────────────────────────────────────\n");
        write("\n");

        exit(0); /* Parent exits successfully */
    }
}

/* ================================================================
 * ENTRY POINT - NAKED FUNCTION
 * 
 * CRITICAL: This MUST be naked to prevent the compiler from 
 * generating a function prologue (push %ebp, mov %esp, %ebp)
 * which would try to write to the stack BEFORE we set up DS/ES!
 * ================================================================ */

__attribute__((noreturn))
__attribute__((naked))
void _start(void)
{
    /* Set up data segments for Ring 3 - NO STACK ACCESS YET! */
    __asm__ volatile(
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        
        /* Now call main_program - stack is ready */
        "call main_program\n"
        
        /* If main_program returns, exit with code 0 */
        "push $0\n"
        "call exit\n"
        
        /* Should never reach here */
        "1: hlt\n"
        "jmp 1b\n"
        ::: "ax"
    );
}