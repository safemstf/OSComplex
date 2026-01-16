#pragma once
#include <stdint.h>

void fpu_init(void);

void isr_device_not_available(uint32_t *stack_ptr);
void isr_x87_fpu_fault(uint32_t *stack_ptr);
void isr_simd_fp_exception(uint32_t *stack_ptr);
