/* interrupts/isr.c - Interrupt Service Routine handlers
 * 
 * This file contains the C functions that handle interrupts after
 * the assembly stubs have saved the CPU state.
 * 
 * FLOW OF AN INTERRUPT:
 * 1. Hardware/CPU triggers interrupt
 * 2. CPU looks up handler in IDT
 * 3. Assembly stub (interrupts.s) saves registers
 * 4. Assembly calls C handler (THIS FILE)
 * 5. C handler does the actual work
 * 6. Return to assembly stub
 * 7. Assembly restores registers and executes IRET
 * 
 * TWO TYPES OF INTERRUPTS:
 * - ISRs (Interrupt Service Routines): CPU exceptions (div by zero, etc.)
 * - IRQs (Interrupt Requests): Hardware interrupts (keyboard, timer, etc.)
 */

#include "../kernel/kernel.h"

/* Exception names for debugging
 * When an exception occurs, we can print a helpful message */
static const char* exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

/* CPU Exception Handler (called from assembly)
 * 
 * This handles CPU faults/exceptions like divide by zero, page faults, etc.
 * 
 * For now, we display an error message and halt. In a real OS, you'd:
 * - Try to recover from the error if possible
 * - Kill the offending process
 * - Log the error for debugging
 * - Potentially restart the system
 * 
 * Parameters passed on stack by assembly stub:
 * - Interrupt number (which exception occurred)
 * - Error code (some exceptions provide this, others get dummy 0)
 */
void isr_handler(void) {
    /* Get the interrupt number from the stack
     * The assembly stub pushed it before calling us */
    uint32_t* stack_ptr;
    __asm__ volatile("mov %%esp, %0" : "=r"(stack_ptr));
    
    /* Calculate offset to interrupt number on stack
     * After: return address (4), saved regs from PUSHA (32), 
     * segment regs (16), error code (4) = 56 bytes */
    uint32_t int_no = stack_ptr[14];
    
    /* Display error message */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("\n\n");
    terminal_writestring("╔══════════════════════════════════════════════════════════╗\n");
    terminal_writestring("║              KERNEL PANIC - CPU Exception                ║\n");
    terminal_writestring("╚══════════════════════════════════════════════════════════╝\n\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("Exception: ");
    
    if (int_no < 32) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring(exception_messages[int_no]);
    } else {
        terminal_writestring("Unknown Exception");
    }
    
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("System halted.\n\n");
    
    /* Halt the system */
    __asm__ volatile("cli; hlt");
    
    /* Infinite loop in case we somehow continue */
    while(1) {
        __asm__ volatile("hlt");
    }
}

/* Array of IRQ handler function pointers
 * 
 * Each hardware device (keyboard, timer, etc.) can register its
 * handler function here. When the IRQ fires, we look up and call
 * the appropriate handler.
 * 
 * This is a common OS design pattern - it allows modular device drivers.
 */
static interrupt_handler_t irq_handlers[16];

/* Register a handler for a specific IRQ
 * 
 * Device drivers call this to "hook" their interrupt handler.
 * For example, keyboard driver calls: irq_install_handler(1, keyboard_handler)
 * 
 * Parameters:
 * - irq: IRQ number (0-15)
 * - handler: Function to call when this IRQ fires
 */
void irq_install_handler(uint8_t irq, interrupt_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
    }
}

/* Unregister a handler for a specific IRQ
 * 
 * Used when a device driver is unloaded or no longer needs interrupts.
 */
void irq_uninstall_handler(uint8_t irq) {
    if (irq < 16) {
        irq_handlers[irq] = 0;
    }
}

/* Main IRQ dispatcher (called from assembly)
 * 
 * This function is called by the assembly IRQ stubs. It:
 * 1. Figures out which IRQ occurred (from stack)
 * 2. Calls the registered handler for that IRQ
 * 3. Sends EOI to the PIC
 * 
 * The assembly stub has pushed the interrupt number on the stack.
 * We need to calculate the IRQ number from it (subtract 32).
 */
void irq_handler(void) {
    /* Get the interrupt number from the stack
     * The assembly stub pushed it before calling us
     * 
     * Stack layout when we're called:
     * [return address]
     * [saved registers from PUSHA]
     * [segment registers]
     * [error code - always 0 for IRQs]
     * [interrupt number] <- We want this
     */
    uint32_t* stack_ptr;
    __asm__ volatile("mov %%esp, %0" : "=r"(stack_ptr));
    
    /* The interrupt number is at a specific offset from ESP
     * After PUSHA (8 regs * 4 bytes = 32) and segment regs (4 * 4 = 16)
     * and error code (4 bytes) = 52 bytes total
     * Plus return address (4 bytes) = 56 bytes from current ESP */
    uint32_t int_no = stack_ptr[14];  /* Approximate - adjust if needed */
    
    /* Convert interrupt number to IRQ number
     * We remapped PIC so: IRQ 0 = INT 32, IRQ 1 = INT 33, etc. */
    uint8_t irq = int_no - 32;
    
    /* Call the registered handler if one exists */
    if (irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq]();
    }
    
    /* CRITICAL: Send EOI to PIC
     * Without this, we won't get any more interrupts from this IRQ!
     * This tells the PIC "I'm done handling this interrupt" */
    pic_send_eoi(irq);
}