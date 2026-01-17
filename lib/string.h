#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/* Basic string/memory functions */
size_t strlen(const char* str);
size_t strnlen(const char* str, size_t maxlen);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
int stricmp(const char* s1, const char* s2);
void* memset(void* dest, int val, size_t len);
void* memcpy(void* dest, const void* src, size_t len);
void* memmove(void* dest, const void* src, size_t len);
int memcmp(const void* s1, const void* s2, size_t n);
void* memchr(const void* s, int c, size_t n);

/* String manipulation */
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);  /* Find LAST occurrence */
char* strstr(const char* haystack, const char* needle);
char* strtok_r(char* str, const char* delim, char** saveptr);
char* strtrim(char* str);

/* Conversion */
void itoa(int n, char* str);
void utoa(uint32_t n, char* str, int base);
void reverse(char* str);

/* Heap-enabled functions (require kmalloc) */
char* strdup(const char* s);
char* strndup(const char* s, size_t n);
char* strconcat(const char* s1, const char* s2);
char* strfmt(const char* fmt, ...);

#endif /* STRING_H */