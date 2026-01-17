/* shell/shell.c - Interactive command shell
 *
 * CLEANED: Test tasks moved to test_tasks.c
 */

#include "../drivers/terminal.h"
#include "../kernel/kernel.h"
#include "../mm/pmm.h"
#include "../interrupts/pagefault.h"
#include "../kernel/scheduler.h"
#include "../kernel/task.h"
#include "test_tasks.h"
#include "../fs/vfs.h"

#define SHELL_BUFFER_SIZE 256
#define SHELL_PROMPT "complex> "

static char command_buffer[SHELL_BUFFER_SIZE];
static size_t command_pos = 0;

#define HISTORY_SIZE 10
static char history[HISTORY_SIZE][SHELL_BUFFER_SIZE] __attribute__((unused));
static size_t history_count = 0;

extern char keyboard_buffer_pop(void);
extern bool keyboard_has_data(void);

static void shell_display_prompt(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring(SHELL_PROMPT);
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

static void cmd_help(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║           OSComplex - Available Commands                ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("System Commands:\n");
    terminal_writestring("  help        - Show this help message\n");
    terminal_writestring("  clear       - Clear the screen\n");
    terminal_writestring("  about       - About OSComplex\n");
    terminal_writestring("  halt        - Shutdown the system\n");

    terminal_writestring("\nMemory Commands:\n");
    terminal_writestring("  meminfo     - Show physical memory usage\n");
    terminal_writestring("  sysinfo     - Show system information\n");
    terminal_writestring("  testpf      - Test page fault recovery\n");
    terminal_writestring("  heaptest    - Test heap allocator\n");

    terminal_writestring("\nTask Commands:\n");
    terminal_writestring("  ps          - List all tasks\n");
    terminal_writestring("  sched       - Show scheduler statistics\n");
    terminal_writestring("  spawn       - Create test tasks\n");

    terminal_writestring("\nOther:\n");
    terminal_writestring("  ai          - Show AI learning statistics\n");
    terminal_writestring("  echo <text> - Display text\n");

    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("💡 AI Tips:\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("  • Press TAB for AI-powered suggestions\n");
    terminal_writestring("  • The AI learns your command patterns\n\n");
}

static void cmd_about(void)
{
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK));
    terminal_writestring("╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║              OSComplex v0.1-alpha                        ║\n");
    terminal_writestring("║           An AI-Native Operating System                 ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("🤖 First OS with AI embedded at the kernel level\n");
    terminal_writestring("🧠 Learns from your usage patterns\n");
    terminal_writestring("⚡ Built from scratch in C and Assembly\n");
    terminal_writestring("🎯 Designed for the future of computing\n");
    terminal_writestring("🔄 Now with multitasking support!\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("Status: Experimental | Learning: Active\n");
    terminal_writestring("Architecture: i686 (32-bit)\n\n");
}

static void cmd_echo(const char *args)
{
    terminal_writestring(args);
    terminal_writestring("\n");
}

static void cmd_meminfo(void)
{
    uint32_t total = pmm_get_total_blocks();
    uint32_t used = pmm_get_used_blocks();
    uint32_t free = pmm_get_free_blocks();

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║               Physical Memory Information               ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("  Total blocks : ");
    terminal_write_dec(total);
    terminal_writestring("\n  Used blocks  : ");
    terminal_write_dec(used);
    terminal_writestring("\n  Free blocks  : ");
    terminal_write_dec(free);
    terminal_writestring("\n\n  Block size   : ");
    terminal_write_dec(PAGE_SIZE);
    terminal_writestring(" bytes\n\n");
}

static void cmd_sysinfo(void)
{
    uint32_t total = pmm_get_total_blocks();
    uint32_t used = pmm_get_used_blocks();
    uint32_t free = pmm_get_free_blocks();

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║              System Information                         ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("Physical Memory:\n");
    terminal_writestring("  Total    : ");
    terminal_write_dec(total * 4);
    terminal_writestring(" KB (");
    terminal_write_dec(total);
    terminal_writestring(" blocks)\n  Used     : ");
    terminal_write_dec(used * 4);
    terminal_writestring(" KB (");
    terminal_write_dec(used);
    terminal_writestring(" blocks)\n  Free     : ");
    terminal_write_dec(free * 4);
    terminal_writestring(" KB (");
    terminal_write_dec(free);
    terminal_writestring(" blocks)\n");

    terminal_writestring("\nMemory Layout:\n");
    terminal_writestring("  Kernel   : 0x00100000 - 0x08000000 (127 MB)\n");
    terminal_writestring("  Heap     : 0xC0400000 - 0xC0800000 (4 MB)\n");

    terminal_writestring("\nSubsystems Status:\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("  [✓] PMM        - Physical Memory Manager\n");
    terminal_writestring("  [✓] VMM        - Virtual Memory Manager\n");
    terminal_writestring("  [✓] Heap       - Kernel Heap Allocator\n");
    terminal_writestring("  [✓] IDT        - Interrupt Descriptor Table\n");
    terminal_writestring("  [✓] PIC        - Programmable Interrupt Controller\n");
    terminal_writestring("  [✓] Timer      - Programmable Interval Timer\n");
    terminal_writestring("  [✓] Task       - Process Management\n");
    terminal_writestring("  [✓] Scheduler  - Round-Robin Scheduler\n");
    terminal_writestring("  [✓] Syscall    - System Call Interface\n");
    terminal_writestring("  [✓] AI         - Learning System\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\n");
}

static void cmd_testpf(void) { test_page_fault_recovery(); }

static void cmd_heaptest(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║              Heap Allocator Test                        ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    terminal_writestring("[TEST 1] Small allocations (64 bytes each)...\n");
    void *ptr1 = kmalloc(64), *ptr2 = kmalloc(64), *ptr3 = kmalloc(64);

    if (ptr1 && ptr2 && ptr3) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("         ✓ All allocations successful\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("         ptr1 = 0x"); terminal_write_hex((uint32_t)ptr1);
        terminal_writestring("\n         ptr2 = 0x"); terminal_write_hex((uint32_t)ptr2);
        terminal_writestring("\n         ptr3 = 0x"); terminal_write_hex((uint32_t)ptr3);
        terminal_writestring("\n");
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("         ✗ Allocation failed\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        return;
    }

    terminal_writestring("\n[TEST 2] Writing to allocated memory...\n");
    uint32_t *test = (uint32_t *)ptr1;
    *test = 0x12345678;

    if (*test == 0x12345678) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("         ✓ Memory write/read successful\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("         Value = 0x"); terminal_write_hex(*test); terminal_writestring("\n");
    }

    terminal_writestring("\n[TEST 3] Freeing memory...\n");
    kfree(ptr1); kfree(ptr2); kfree(ptr3);
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("         ✓ Memory freed successfully\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\n[TEST 4] Large allocation (8KB)...\n");
    void *large = kmalloc(8192);

    if (large) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("         ✓ Large allocation successful\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("         ptr = 0x"); terminal_write_hex((uint32_t)large); terminal_writestring("\n");
        kfree(large);
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("         ✓ Large memory freed\n");
    }

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\n");
}

static void cmd_ps(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║                    Task List                             ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("PID  STATE    NAME\n");
    terminal_writestring("---  -------  --------------------\n");

    task_t *task = kernel_task;
    while (task) {
        char buf[16];
        itoa(task->pid, buf);
        terminal_writestring(buf);
        terminal_writestring(task->pid < 10 ? "    " : (task->pid < 100 ? "   " : "  "));

        switch (task->state) {
        case TASK_READY:    terminal_writestring("READY  "); break;
        case TASK_RUNNING:  terminal_writestring("RUN    "); break;
        case TASK_BLOCKED:  terminal_writestring("BLOCK  "); break;
        case TASK_SLEEPING: terminal_writestring("SLEEP  "); break;
        case TASK_ZOMBIE:   terminal_writestring("ZOMBIE "); break;
        default:            terminal_writestring("???    "); break;
        }
        terminal_writestring(" ");
        terminal_writestring(task->name);
        terminal_writestring("\n");

        task = task->next;
    }

    terminal_writestring("\n");
}

static void cmd_sched(void)
{
    scheduler_stats_t stats = scheduler_get_stats();

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║               Scheduler Statistics                       ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    char buf[16];
    terminal_writestring("Total tasks       : "); itoa(stats.total_tasks, buf); terminal_writestring(buf); terminal_writestring("\n");
    terminal_writestring("Ready tasks       : "); itoa(stats.ready_tasks, buf); terminal_writestring(buf); terminal_writestring("\n");
    terminal_writestring("Blocked tasks     : "); itoa(stats.blocked_tasks, buf); terminal_writestring(buf); terminal_writestring("\n");
    terminal_writestring("Context switches  : "); itoa(stats.context_switches, buf); terminal_writestring(buf); terminal_writestring("\n");
    terminal_writestring("Total ticks       : "); itoa(stats.total_ticks, buf); terminal_writestring(buf); terminal_writestring("\n\n");
}

static void cmd_spawn(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n[SPAWN] Creating test tasks...\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    task_t *t1 = task_create("syscall_test", syscall_test_task, 10);
    if (t1) scheduler_add_task(t1);

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[SPAWN] Syscall test task created!\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\n");
}

static void cmd_halt(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║                System Shutdown Initiated                ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("Goodbye! System halted.\n(Close QEMU window or press Ctrl+C)\n\n");

    __asm__ volatile("cli; hlt");
}

static void cmd_ls(const char *path)
{
    vfs_node_t *dir;

    if (!path || !*path) {
        dir = vfs_cwd;
    } else {
        dir = vfs_open_path(path);
    }

    if (!dir || dir->type != VFS_DIRECTORY) {
        terminal_writestring("ls: not a directory\n");
        return;
    }

    for (uint32_t i = 0;; i++) {
        dirent_t *ent = dir->ops->readdir(dir, i);
        if (!ent) break;

        terminal_writestring(ent->name);
        terminal_writestring("  ");
    }

    terminal_writestring("\n");
}

static void cmd_pwd(void)
{
    const char *cwd = vfs_getcwd();
    if (cwd) {
        terminal_writestring(cwd);
        terminal_writestring("\n");
    }
}

static void cmd_cd(const char *path)
{
    if (!path || !*path) {
        terminal_writestring("cd: missing operand\n");
        return;
    }

    if (vfs_chdir(path) < 0) {
        terminal_writestring("cd: no such directory\n");
    }
}

static void cmd_mkdir(const char *path)
{
    if (!path || !*path) {
        terminal_writestring("mkdir: missing operand\n");
        return;
    }

    if (vfs_mkdir(path, S_IRWXU) < 0) {
        terminal_writestring("mkdir: failed\n");
    }
}

static void shell_execute_command(const char *cmd)
{
    if (!cmd || !*cmd) return;

    bool success = false;
    const char *args = cmd;
    while (*args && *args != ' ') args++;
    if (*args == ' ') args++;

    if (strcmp(cmd, "help") == 0)         { cmd_help(); success = true; }
    else if (strcmp(cmd, "clear") == 0)   { terminal_clear(); success = true; }
    else if (strcmp(cmd, "about") == 0)   { cmd_about(); success = true; }
    else if (strcmp(cmd, "ai") == 0)      { ai_show_stats(); success = true; }
    else if (strncmp(cmd, "echo ", 5) == 0) { cmd_echo(args); success = true; }
    else if (strcmp(cmd, "meminfo") == 0) { cmd_meminfo(); success = true; }
    else if (strcmp(cmd, "sysinfo") == 0) { cmd_sysinfo(); success = true; }
    else if (strcmp(cmd, "testpf") == 0)  { cmd_testpf(); success = true; }
    else if (strcmp(cmd, "heaptest") == 0) { cmd_heaptest(); success = true; }
    else if (strcmp(cmd, "ps") == 0)      { cmd_ps(); success = true; }
    else if (strcmp(cmd, "sched") == 0)   { cmd_sched(); success = true; }
    else if (strcmp(cmd, "spawn") == 0)   { cmd_spawn(); success = true; }
    else if (strcmp(cmd, "halt") == 0)    { cmd_halt(); }

    // vfs cmds
    else if (strcmp(cmd, "ls") == 0)           { cmd_ls(NULL); success = true; }
    else if (strncmp(cmd, "ls ", 3) == 0)      { cmd_ls(args); success = true; }
    else if (strcmp(cmd, "pwd") == 0)          { cmd_pwd(); success = true; }
    else if (strncmp(cmd, "cd ", 3) == 0)      { cmd_cd(args); success = true; }
    else if (strncmp(cmd, "mkdir ", 6) == 0)   { cmd_mkdir(args); success = true; }


    else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Unknown command: ");
        terminal_writestring(cmd);
        terminal_writestring("\n");

        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("Type 'help' for available commands\n");

        const char *suggestion = ai_predict_command(cmd);
        if (suggestion) {
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
            terminal_writestring("[AI] Did you mean: ");
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
            terminal_writestring(suggestion);
            terminal_writestring("?\n");
        }
    }

    ai_learn_command(cmd, success);
}

static void shell_process_input(void)
{
    if (!keyboard_has_data()) return;

    char c = keyboard_buffer_pop();
    if (!c) return;

    switch (c) {
    case '\n':
        terminal_putchar('\n');
        command_buffer[command_pos] = '\0';
        shell_execute_command(command_buffer);
        command_pos = 0;
        shell_display_prompt();
        break;

    case '\b':
        if (command_pos > 0) {
            command_pos--;
            terminal_putchar('\b');
        }
        break;

    case '\t':
        command_buffer[command_pos] = '\0';
        ai_show_suggestions(command_buffer);
        shell_display_prompt();
        for (size_t i = 0; i < command_pos; i++)
            terminal_putchar(command_buffer[i]);
        break;

    default:
        if (command_pos < SHELL_BUFFER_SIZE - 1) {
            command_buffer[command_pos++] = c;
            terminal_putchar(c);
        }
        break;
    }
}

void shell_init(void)
{
    command_pos = 0;
    history_count = 0;
    memset(command_buffer, 0, sizeof(command_buffer));

    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[SHELL] Interactive shell ready\n");
    terminal_writestring("[SHELL] Type 'help' for available commands\n\n");
}

void shell_run(void)
{
    shell_display_prompt();

    while (1) {
        shell_process_input();
        __asm__ volatile("hlt");
    }
}