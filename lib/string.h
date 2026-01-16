#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>

/* Standard string/memory functions from your string.c */
size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
void* memset(void* dest, int val, size_t len);
void* memcpy(void* dest, const void* src, size_t len);

/* The new itoa function you added */
void itoa(int n, char* str);
void reverse(char* str); // Useful helper for itoa

#endif