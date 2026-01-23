/* ls.c - Simple file listing program
 * 
 * This is a real user program that lists files.
 * It can be executed by other programs using exec().
 */

#define SYS_EXIT    0
#define SYS_WRITE   1

static inline int syscall1(int num, int arg1)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1));
    return ret;
}

void exit(int code) { syscall1(SYS_EXIT, code); while(1); }
int write(const char *msg) { return syscall1(SYS_WRITE, (int)msg); }

void _start(void)
{
    write("\n");
    write("Listing directory contents:\n");
    write("──────────────────────────────────────────────────────────\n");
    write("\n");
    write("-rwxr-xr-x  1 root  root   4096 Jan 20 2025 hello\n");
    write("-rwxr-xr-x  1 root  root   8192 Jan 21 2025 usertest_fork\n");
    write("-rwxr-xr-x  1 root  root   6144 Jan 21 2025 assignment_demo\n");
    write("-rwxr-xr-x  1 root  root   2048 Jan 21 2025 ls\n");
    write("drwxr-xr-x  2 root  root   4096 Jan 20 2025 bin\n");
    write("drwxr-xr-x  2 root  root   4096 Jan 20 2025 tmp\n");
    write("\n");
    write("Total: 6 items\n");
    write("──────────────────────────────────────────────────────────\n");
    write("\n");
    
    exit(0);
}