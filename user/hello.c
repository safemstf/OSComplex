/* user/hello.c - First User Mode Program
 * 
 * This program runs in Ring 3 (user mode) and demonstrates:
 * - System calls via INT 0x80
 * - Writing to screen via SYS_WRITE
 * - Clean exit via SYS_EXIT
 */

/* System call numbers - must match kernel/syscall.h */
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_YIELD   3
#define SYS_GETPID  4

/* Entry point - called by kernel when program starts
 * 
 * NOTE: No main()! We use _start() because we don't have a C runtime library.
 * The kernel jumps directly here after loading the program.
 */
void _start(void) {
    const char *msg = "Hello from user mode!\n";
    
    /* Make SYS_WRITE syscall
     * 
     * EAX = 1 (SYS_WRITE)
     * EBX = pointer to message
     * 
     * This will trigger INT 0x80, which:
     * 1. Switches from Ring 3 to Ring 0
     * 2. Calls syscall_handler in kernel
     * 3. Dispatches to sys_write()
     * 4. Returns to user mode
     */
    __asm__ volatile(
        "mov $1, %%eax\n"      /* SYS_WRITE */
        "mov %0, %%ebx\n"      /* message pointer */
        "int $0x80\n"          /* trigger syscall */
        :: "r"(msg)
        : "eax", "ebx"
    );
    
    /* Make SYS_EXIT syscall
     * 
     * EAX = 0 (SYS_EXIT)
     * EBX = 0 (exit code - success)
     * 
     * This tells the kernel we're done and want to terminate.
     */
    __asm__ volatile(
        "mov $0, %%eax\n"      /* SYS_EXIT */
        "mov $0, %%ebx\n"      /* exit code 0 */
        "int $0x80\n"          /* trigger syscall */
        :
        :
        : "eax", "ebx"
    );
    
    /* Should never reach here! SYS_EXIT doesn't return. */
    while(1);
}