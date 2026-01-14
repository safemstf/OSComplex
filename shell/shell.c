/* shell/shell.c - Interactive command shell with AI assistance
 * 
 * This is the user-facing interface to LinuxComplex. It provides:
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

#include "../kernel/kernel.h"

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
static void shell_display_prompt(void) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring(SHELL_PROMPT);
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

/* Built-in command: help
 * Shows available commands and AI tips */
static void cmd_help(void) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("\n╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║           LinuxComplex - Available Commands             ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("  help        - Show this help message\n");
    terminal_writestring("  clear       - Clear the screen\n");
    terminal_writestring("  about       - About LinuxComplex\n");
    terminal_writestring("  ai          - Show AI learning statistics\n");
    terminal_writestring("  echo <text> - Display text\n");
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
 * Information about LinuxComplex */
static void cmd_about(void) {
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK));
    terminal_writestring("╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║              LinuxComplex v0.1-alpha                     ║\n");
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
static void cmd_echo(const char* args) {
    terminal_writestring(args);
    terminal_writestring("\n");
}

/* Built-in command: halt
 * Shutdown the system */
static void cmd_halt(void) {
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
static void shell_execute_command(const char* cmd) {
    /* Skip empty commands */
    if (!cmd || !*cmd) return;
    
    bool success = false;
    
    /* Parse command - find first space to separate command from args */
    const char* args = cmd;
    while (*args && *args != ' ') args++;
    if (*args == ' ') args++;  /* Skip the space */
    
    /* Match against built-in commands */
    if (strcmp(cmd, "help") == 0) {
        cmd_help();
        success = true;
    }
    else if (strcmp(cmd, "clear") == 0) {
        terminal_clear();
        success = true;
    }
    else if (strcmp(cmd, "about") == 0) {
        cmd_about();
        success = true;
    }
    else if (strcmp(cmd, "ai") == 0) {
        ai_show_stats();
        success = true;
    }
    else if (strncmp(cmd, "echo ", 5) == 0) {
        cmd_echo(args);
        success = true;
    }
    else if (strcmp(cmd, "halt") == 0) {
        cmd_halt();
        /* Never returns */
    }
    else {
        /* Unknown command */
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Unknown command: ");
        terminal_writestring(cmd);
        terminal_writestring("\n");
        
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("Type 'help' for available commands\n");
        
        /* Ask AI for suggestions */
        const char* suggestion = ai_predict_command(cmd);
        if (suggestion) {
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
static void shell_process_input(void) {
    if (!keyboard_has_data()) return;
    
    char c = keyboard_buffer_pop();
    if (!c) return;
    
    switch (c) {
        case '\n':  /* Enter - execute command */
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
            
        case '\b':  /* Backspace */
            if (command_pos > 0) {
                command_pos--;
                terminal_putchar('\b');
            }
            break;
            
        case '\t':  /* Tab - AI suggestions */
            command_buffer[command_pos] = '\0';
            ai_show_suggestions(command_buffer);
            
            /* Redisplay prompt and current input */
            shell_display_prompt();
            for (size_t i = 0; i < command_pos; i++) {
                terminal_putchar(command_buffer[i]);
            }
            break;
            
        default:
            /* Regular character - add to buffer if space available */
            if (command_pos < SHELL_BUFFER_SIZE - 1) {
                command_buffer[command_pos++] = c;
                terminal_putchar(c);
            }
            break;
    }
}

/* Initialize shell subsystem */
void shell_init(void) {
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
void shell_run(void) {
    /* Display first prompt */
    shell_display_prompt();
    
    /* Main loop - process input forever */
    while (1) {
        shell_process_input();
        
        /* Halt CPU until next interrupt
         * This saves power and reduces heat */
        __asm__ volatile("hlt");
    }
}