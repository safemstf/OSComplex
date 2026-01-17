/* interrupts/pagefault.h */

#ifndef PAGEFAULT_H
#define PAGEFAULT_H

#include <stdint.h>

void page_fault_handler(uint32_t *stack_ptr);
void test_page_fault_recovery(void);  /* Test function for shell */

#endif /* PAGEFAULT_H */