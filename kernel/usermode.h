/* kernel/usermode.h - User Mode Transition
 * 
 * Functions for switching between kernel and user mode.
 */

#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

/* Jump to user mode and execute function
 * 
 * entry_point: Address of user function to execute
 * user_stack: Top of user stack (grows down)
 * 
 * This function does not return - execution continues in user mode
 * until the program makes a syscall or triggers an exception.
 */
void enter_usermode(uint32_t entry_point, uint32_t user_stack) __attribute__((noreturn));

#endif /* USERMODE_H */