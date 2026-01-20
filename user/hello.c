// user/hello.c
void _start() {
    const char *msg = "Hello from user mode!\n";
    asm volatile(
        "mov $1, %%eax\n"      // SYS_WRITE
        "mov %0, %%ebx\n"      // message
        "int $0x80\n"
        :: "r"(msg) : "eax", "ebx"
    );
    
    asm volatile(
        "mov $0, %%eax\n"      // SYS_EXIT
        "mov $0, %%ebx\n"      // exit code 0
        "int $0x80\n"
    );
}