/* shell/test_tasks.c - Test tasks for demonstrating multitasking
 * 
 * Contains various test tasks that can be spawned to demonstrate
 * the OS's multitasking capabilities.
 */

#include "../kernel/kernel.h"
#include "../kernel/task.h"
#include "../drivers/terminal.h"

/* ================================================================
 * BASIC TEST TASKS
 * ================================================================ */

void test_task1(void)
{
    for (int i = 0; i < 5; i++) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("[TASK1] Running iteration ");
        char buf[16];
        itoa(i, buf);
        terminal_writestring(buf);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

        /* Busy wait to simulate work */
        for (volatile int j = 0; j < 1000000; j++);
    }

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[TASK1] Finished!\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    task_exit(0);
}

void test_task2(void)
{
    for (int i = 0; i < 5; i++) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK));
        terminal_writestring("[TASK2] Running iteration ");
        char buf[16];
        itoa(i, buf);
        terminal_writestring(buf);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

        /* Busy wait */
        for (volatile int j = 0; j < 1000000; j++);
    }

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[TASK2] Finished!\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    task_exit(0);
}

/* ================================================================
 * SYSCALL TEST TASK
 * ================================================================ */

void syscall_test_task(void)
{
    /* Get our PID using syscall */
    uint32_t pid;
    asm volatile(
        "mov $4, %%eax\n"   /* SYS_GETPID */
        "int $0x80\n"
        : "=a"(pid)
    );

    /* Build message string */
    char msg[64];
    msg[0] = '[';
    msg[1] = 'P';
    msg[2] = 'I';
    msg[3] = 'D';
    msg[4] = ' ';
    msg[5] = '0' + pid;
    msg[6] = ']';
    msg[7] = ' ';
    msg[8] = 'H';
    msg[9] = 'e';
    msg[10] = 'l';
    msg[11] = 'l';
    msg[12] = 'o';
    msg[13] = ' ';
    msg[14] = 'f';
    msg[15] = 'r';
    msg[16] = 'o';
    msg[17] = 'm';
    msg[18] = ' ';
    msg[19] = 's';
    msg[20] = 'y';
    msg[21] = 's';
    msg[22] = 'c';
    msg[23] = 'a';
    msg[24] = 'l';
    msg[25] = 'l';
    msg[26] = '!';
    msg[27] = '\n';
    msg[28] = '\0';

    /* Write using syscall */
    asm volatile(
        "mov $1, %%eax\n"      /* SYS_WRITE */
        "mov %0, %%ebx\n"      /* msg pointer */
        "int $0x80\n"
        :
        : "r"(msg)
        : "eax", "ebx"
    );

    /* Yield a few times */
    for (int i = 0; i < 3; i++) {
        asm volatile(
            "mov $3, %%eax\n"  /* SYS_YIELD */
            "int $0x80\n"
            ::: "eax"
        );
    }

    /* Exit using syscall */
    asm volatile(
        "mov $0, %%eax\n"      /* SYS_EXIT */
        "mov $0, %%ebx\n"      /* exit code 0 */
        "int $0x80\n"
        ::: "eax", "ebx"
    );
}