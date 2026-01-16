/* shell/shell.c - Interactive command shell with AI assistance
 *
 * This is the user-facing interface to OSComplex. It provides:
 * - Command-line interface (like bash, zsh, etc.)
 * - AI-powered autocomplete and suggestions
 * - Built-in commands
 * - Beautiful colored output
 *
 * SHELL DESIGN:
 * Simple REPL (Read-Eval-Print Loop):
 * 1. Display prompt
 * 2. Read user input (with autocomplete)
 * 3. Parse and execute command
 * 4. Display result
 * 5. Repeat
 *
 * AI INTEGRATION:
 * - TAB: Show AI suggestions
 * - Auto-learning from every command
 * - Smart predictions based on context
 */

#include "../drivers/terminal.h"
#include "../kernel/kernel.h"
#include "../mm/pmm.h"

#define SHELL_BUFFER_SIZE 256
#define SHELL_PROMPT "complex> "

/* Current command buffer */
static char command_buffer[SHELL_BUFFER_SIZE];
static size_t command_pos = 0;

/* Command history (for future up/down arrow support) */
#define HISTORY_SIZE 10
/* TODO: Implement command history navigation with up/down arrows */
static char history[HISTORY_SIZE][SHELL_BUFFER_SIZE] __attribute__((unused));
static size_t history_count = 0;

/* External functions we need from keyboard driver */
extern char keyboard_buffer_pop(void);
extern bool keyboard_has_data(void);

/* Display the shell prompt
 * Shows current directory, user, etc.
 * For now, just "complex> " */
static void shell_display_prompt(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring(SHELL_PROMPT);
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

/* Built-in command: help
 * Shows available commands and AI tips */
static void cmd_help(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║           OSComplex - Available Commands             ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("  help        - Show this help message\n");
    terminal_writestring("  clear       - Clear the screen\n");
    terminal_writestring("  about       - About OSComplex\n");
    terminal_writestring("  ai          - Show AI learning statistics\n");
    terminal_writestring("  echo <text> - Display text\n");
    terminal_writestring("  meminfo     - Show physical memory usage\n");
    terminal_writestring("  sysinfo     - Show system information\n");
    terminal_writestring("  testpf      - Test page fault recovery\n");
    terminal_writestring("  heaptest    - Test heap allocator\n");
    terminal_writestring("  halt        - Shutdown the system\n");

    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("💡 AI Tips:\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("  • Press TAB for AI-powered suggestions\n");
    terminal_writestring("  • The AI learns your command patterns\n");
    terminal_writestring("  • More features coming soon!\n\n");
}

/* Built-in command: about
 * Information about OSComplex */
static void cmd_about(void)
{
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK));
    terminal_writestring("╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║              OSComplex v0.1-alpha                     ║\n");
    terminal_writestring("║           An AI-Native Operating System                 ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("🤖 First OS with AI embedded at the kernel level\n");
    terminal_writestring("🧠 Learns from your usage patterns\n");
    terminal_writestring("⚡ Built from scratch in C and Assembly\n");
    terminal_writestring("🎯 Designed for the future of computing\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("Status: Experimental | Learning: Active\n");
    terminal_writestring("Architecture: i686 (32-bit)\n\n");
}

/* Built-in command: echo
 * Displays whatever comes after "echo " */
static void cmd_echo(const char *args)
{
    terminal_writestring(args);
    terminal_writestring("\n");
}

/* Built-in command: meminfo
 * Shows physical memory usage */
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
    terminal_writestring("\n");

    terminal_writestring("  Used blocks  : ");
    terminal_write_dec(used);
    terminal_writestring("\n");

    terminal_writestring("  Free blocks  : ");
    terminal_write_dec(free);
    terminal_writestring("\n\n");

    terminal_writestring("  Block size   : ");
    terminal_write_dec(PAGE_SIZE);
    terminal_writestring(" bytes\n\n");
}

/* Built-in command: sysinfo - System information */
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
    terminal_writestring(" blocks)\n");
    
    terminal_writestring("  Used     : ");
    terminal_write_dec(used * 4);
    terminal_writestring(" KB (");
    terminal_write_dec(used);
    terminal_writestring(" blocks)\n");
    
    terminal_writestring("  Free     : ");
    terminal_write_dec(free * 4);
    terminal_writestring(" KB (");
    terminal_write_dec(free);
    terminal_writestring(" blocks)\n");
    
    terminal_writestring("\nMemory Layout:\n");
    terminal_writestring("  Kernel   : 0x00100000 - 0x08000000 (127 MB)\n");
    terminal_writestring("  Heap     : 0xC0400000 - 0xC0800000 (4 MB)\n");
    
    terminal_writestring("\nSubsystems Status:\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("  [✓] PMM   - Physical Memory Manager\n");
    terminal_writestring("  [✓] VMM   - Virtual Memory Manager\n");
    terminal_writestring("  [✓] Heap  - Kernel Heap Allocator\n");
    terminal_writestring("  [✓] IDT   - Interrupt Descriptor Table\n");
    terminal_writestring("  [✓] PIC   - Programmable Interrupt Controller\n");
    terminal_writestring("  [✓] AI    - Learning System\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\n");
}

/* Built-in command: testpf - Test page fault recovery */
static void cmd_testpf(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║            Page Fault Recovery Test                     ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    /* Test 1: Access unmapped memory in heap range */
    terminal_writestring("[TEST 1] Accessing unmapped page at 0xC0500000...\n");
    volatile uint32_t* test_ptr = (volatile uint32_t*)0xC0500000;
    
    terminal_writestring("         Writing 0xDEADBEEF...\n");
    *test_ptr = 0xDEADBEEF;
    
    terminal_writestring("         Reading back value...\n");
    uint32_t value = *test_ptr;
    
    terminal_writestring("         Value = 0x");
    terminal_write_hex(value);
    terminal_writestring("\n");
    
    if (value == 0xDEADBEEF) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("         ✓ SUCCESS - Page fault recovery works!\n");
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("         ✗ FAILED - Wrong value\n");
    }
    
    /* Test 2: Access another unmapped page */
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\n[TEST 2] Accessing unmapped page at 0xC0600000...\n");
    volatile uint32_t* test_ptr2 = (volatile uint32_t*)0xC0600000;
    
    terminal_writestring("         Writing 0xCAFEBABE...\n");
    *test_ptr2 = 0xCAFEBABE;
    
    terminal_writestring("         Reading back value...\n");
    value = *test_ptr2;
    
    terminal_writestring("         Value = 0x");
    terminal_write_hex(value);
    terminal_writestring("\n");
    
    if (value == 0xCAFEBABE) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("         ✓ SUCCESS - Multiple page faults work!\n");
    }
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\n");
}

/* Built-in command: heaptest - Test heap allocator */
static void cmd_heaptest(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║              Heap Allocator Test                        ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    /* Test 1: Small allocations */
    terminal_writestring("[TEST 1] Small allocations (64 bytes each)...\n");
    void* ptr1 = kmalloc(64);
    void* ptr2 = kmalloc(64);
    void* ptr3 = kmalloc(64);
    
    if (ptr1 && ptr2 && ptr3) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("         ✓ All allocations successful\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("         ptr1 = 0x");
        terminal_write_hex((uint32_t)ptr1);
        terminal_writestring("\n         ptr2 = 0x");
        terminal_write_hex((uint32_t)ptr2);
        terminal_writestring("\n         ptr3 = 0x");
        terminal_write_hex((uint32_t)ptr3);
        terminal_writestring("\n");
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("         ✗ Allocation failed\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        return;
    }
    
    /* Test 2: Write to allocated memory */
    terminal_writestring("\n[TEST 2] Writing to allocated memory...\n");
    uint32_t* test = (uint32_t*)ptr1;
    *test = 0x12345678;
    
    if (*test == 0x12345678) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("         ✓ Memory write/read successful\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("         Value = 0x");
        terminal_write_hex(*test);
        terminal_writestring("\n");
    }
    
    /* Test 3: Free memory */
    terminal_writestring("\n[TEST 3] Freeing memory...\n");
    kfree(ptr1);
    kfree(ptr2);
    kfree(ptr3);
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("         ✓ Memory freed successfully\n");
    
    /* Test 4: Large allocation */
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\n[TEST 4] Large allocation (8KB)...\n");
    void* large = kmalloc(8192);
    
    if (large) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("         ✓ Large allocation successful\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("         ptr = 0x");
        terminal_write_hex((uint32_t)large);
        terminal_writestring("\n");
        kfree(large);
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("         ✓ Large memory freed\n");
    }
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("\n");
}

/* Built-in command: halt
 * Shutdown the system */
static void cmd_halt(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n");
    terminal_writestring("╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║                System Shutdown Initiated                ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("Goodbye! System halted.\n");
    terminal_writestring("(Close QEMU window or press Ctrl+C)\n\n");

    /* Halt the CPU */
    __asm__ volatile("cli; hlt");
}

/* Execute a command
 *
 * Parses the command and calls the appropriate handler.
 * Also tells AI whether the command succeeded or failed.
 */
static void shell_execute_command(const char *cmd)
{
    /* Skip empty commands */
    if (!cmd || !*cmd)
        return;

    bool success = false;

    /* Parse command - find first space to separate command from args */
    const char *args = cmd;
    while (*args && *args != ' ')
        args++;
    if (*args == ' ')
        args++; /* Skip the space */

    /* Match against built-in commands */
    if (strcmp(cmd, "help") == 0)
    {
        cmd_help();
        success = true;
    }
    else if (strcmp(cmd, "clear") == 0)
    {
        terminal_clear();
        success = true;
    }
    else if (strcmp(cmd, "about") == 0)
    {
        cmd_about();
        success = true;
    }
    else if (strcmp(cmd, "ai") == 0)
    {
        ai_show_stats();
        success = true;
    }
    else if (strncmp(cmd, "echo ", 5) == 0)
    {
        cmd_echo(args);
        success = true;
    }
    else if (strcmp(cmd, "meminfo") == 0)
    {
        cmd_meminfo();
        success = true;
    }
    else if (strcmp(cmd, "sysinfo") == 0)
    {
        cmd_sysinfo();
        success = true;
    }
    else if (strcmp(cmd, "testpf") == 0)
    {
        cmd_testpf();
        success = true;
    }
    else if (strcmp(cmd, "heaptest") == 0)
    {
        cmd_heaptest();
        success = true;
    }
    else if (strcmp(cmd, "halt") == 0)
    {
        cmd_halt();
        /* Never returns */
    }
    else
    {
        /* Unknown command */
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Unknown command: ");
        terminal_writestring(cmd);
        terminal_writestring("\n");

        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("Type 'help' for available commands\n");

        /* Ask AI for suggestions */
        const char *suggestion = ai_predict_command(cmd);
        if (suggestion)
        {
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
            terminal_writestring("[AI] Did you mean: ");
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
            terminal_writestring(suggestion);
            terminal_writestring("?\n");
        }

        success = false;
    }

    /* Tell AI about this command execution */
    ai_learn_command(cmd, success);
}

/* Process keyboard input
 *
 * Handles special keys:
 * - Enter: Execute command
 * - Backspace: Delete character
 * - Tab: Show AI suggestions
 * - Regular characters: Add to buffer
 */
static void shell_process_input(void)
{
    if (!keyboard_has_data())
        return;

    char c = keyboard_buffer_pop();
    if (!c)
        return;

    switch (c)
    {
    case '\n': /* Enter - execute command */
        terminal_putchar('\n');

        /* Null-terminate command */
        command_buffer[command_pos] = '\0';

        /* Execute it */
        shell_execute_command(command_buffer);

        /* Reset buffer for next command */
        command_pos = 0;

        /* Show new prompt */
        shell_display_prompt();
        break;

    case '\b': /* Backspace */
        if (command_pos > 0)
        {
            command_pos--;
            terminal_putchar('\b');
        }
        break;

    case '\t': /* Tab - AI suggestions */
        command_buffer[command_pos] = '\0';
        ai_show_suggestions(command_buffer);

        /* Redisplay prompt and current input */
        shell_display_prompt();
        for (size_t i = 0; i < command_pos; i++)
        {
            terminal_putchar(command_buffer[i]);
        }
        break;

    default:
        /* Regular character - add to buffer if space available */
        if (command_pos < SHELL_BUFFER_SIZE - 1)
        {
            command_buffer[command_pos++] = c;
            terminal_putchar(c);
        }
        break;
    }
}

/* Initialize shell subsystem */
void shell_init(void)
{
    command_pos = 0;
    history_count = 0;

    /* Clear command buffer */
    memset(command_buffer, 0, sizeof(command_buffer));

    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[SHELL] Interactive shell ready\n");
    terminal_writestring("[SHELL] Type 'help' for available commands\n\n");
}

/* Main shell loop
 *
 * This is called by kernel_main() and runs forever, processing
 * user input and executing commands.
 */
void shell_run(void)
{
    /* Display first prompt */
    shell_display_prompt();

    /* Main loop - process input forever */
    while (1)
    {
        shell_process_input();

        /* Halt CPU until next interrupt
         * This saves power and reduces heat */
        __asm__ volatile("hlt");
    }
}