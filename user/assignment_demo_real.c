/* assignment_demo_real.c - Fork/Exec/Wait with REAL exec()
 *
 * This version actually uses exec() to load and run the ls program.
 * This is closer to how Unix actually works.
 *
 * Requirements:
 * - Compile ls.c separately as /bin/ls
 * - This program will fork() and exec("/bin/ls") in the child
 */

/* ================================================================
 * FORWARD DECLARATIONS
 * ================================================================ */
static void main_program(void);
static void exit(int code);

/* ================================================================
 * ENTRY POINT - NAKED FUNCTION (MUST BE FIRST!)
 *
 * CRITICAL: This MUST be naked to prevent the compiler from
 * generating a function prologue (push %ebp, mov %esp, %ebp)
 * which would try to access memory using DS before we set it up!
 *
 * After IRET to user mode:
 *   CS = 0x1B (user code)
 *   SS = 0x23 (user data)
 *   DS, ES, FS, GS = still kernel segments (0x10)!
 *
 * We MUST set DS/ES/FS/GS to 0x23 before ANY memory access.
 * ================================================================ */

__attribute__((section(".text.startup")))
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

static int fork(void) { return syscall0(SYS_FORK); }
static int wait(int *status) { return syscall1(SYS_WAIT, (int)status); }
static void exit(int code)
{
    syscall1(SYS_EXIT, code);
    while (1)
        ;
}
static int getpid(void) { return syscall0(SYS_GETPID); }
static int write(const char *msg) { return syscall1(SYS_WRITE, (int)msg); }
static int exec(const char *path) { return syscall1(SYS_EXEC, (int)path); }

/* ================================================================
 * HELPER FUNCTIONS
 * ================================================================ */

static void print_num(int n)
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
 * execlp() SIMULATION
 *
 * In Unix: execlp("ls", "ls", "-l", NULL);
 *
 * We don't support arguments yet, so we'll just exec the program.
 * In a full implementation, you'd pass args through exec().
 * ================================================================ */

static int execlp(const char *file, const char *arg0, ...)
{
    /* For now, we ignore arguments and just exec the program */
    (void)arg0;

    /* Try to find the program in /bin/ */
    char path[32]; /* Shorter buffer for short names */
    int i = 0;

    /* Build path: /bin/file */
    path[i++] = '/';
    path[i++] = 'b';
    path[i++] = 'i';
    path[i++] = 'n';
    path[i++] = '/';

    /* Copy filename (max 8 chars for FAT16) */
    const char *f = file;
    int copied = 0;
    while (*f && i < 31 && copied < 8)
    {
        path[i++] = *f++;
        copied++;
    }
    path[i] = '\0';

    /* Execute the program */
    return exec(path);
}

/* ================================================================
 * MAIN PROGRAM
 * ================================================================ */

static void main_program(void)
{
    write("\n");
    write("╔══════════════════════════════════════════════════════════╗\n");
    write("║     Fork/Exec/Wait Assignment (REAL EXEC VERSION)        ║\n");
    write("╚══════════════════════════════════════════════════════════╝\n");
    write("\n");

    write("PARENT PROCESS:\n");
    write("──────────────\n");
    write("Parent PID: ");
    print_num(getpid());
    write("\n\n");

    write("Creating child process with fork()...\n");

    int pid = fork();

    if (pid < 0)
    {
        write("\nERROR: Fork failed!\n\n");
        exit(1);
    }

    if (pid == 0)
    {
        /* CHILD PROCESS */
        write("\n");
        write("═══════════════════════════════════════════════════════════\n");
        write("  CHILD PROCESS\n");
        write("═══════════════════════════════════════════════════════════\n");
        write("\n");

        write("Child PID: ");
        print_num(getpid());
        write("\n\n");

        write("Child executing: execlp(\"ls\", \"ls\", \"-l\", NULL)\n");
        write("\n");

        /* This replaces the child's memory with the ls program */
        execlp("ls", "ls", "-l", (char *)0);

        /* If we reach here, exec failed */
        write("\nERROR: exec() failed!\n");
        write("Could not execute /bin/ls\n");
        write("\n");
        exit(1);
    }
    else
    {
        /* PARENT PROCESS */
        write("Fork successful! Child PID: ");
        print_num(pid);
        write("\n\n");

        write("Parent waiting for child (wait() syscall)...\n\n");

        int status = 0;
        int child_pid = wait(&status);

        write("\n");
        write("═══════════════════════════════════════════════════════════\n");
        write("  PARENT RESUMED\n");
        write("═══════════════════════════════════════════════════════════\n");
        write("\n");

        write("Child completed! Reaped PID: ");
        print_num(child_pid);
        write("\n");
        write("Exit status: ");
        print_num(status);
        write("\n\n");

        if (status == 0)
        {
            write("✓ SUCCESS: Child executed command and exited normally\n");
        }
        else
        {
            write("✗ FAILED: Child exited with error code ");
            print_num(status);
            write("\n");
        }

        write("\n");
        write("──────────────────────────────────────────────────────────\n");
        write("Assignment Complete!\n");
        write("──────────────────────────────────────────────────────────\n");
        write("\n");

        exit(0);
    }
}
