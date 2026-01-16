// /* string.c - Basic string manipulation functions
//  *
// //  * Since we're writing an OS, we can't use libc. We need to implement
//  * our own string functions. These are simplified versions of the
//  * standard C library functions.
//  */

#include "../kernel/kernel.h"

/* Calculate length of a null-terminated string
 *
 * Why we need this: Constantly used for string operations, printing, etc.
 * Implementation: Simple loop until we hit '\0'
 * Complexity: O(n) where n is string length
 */
size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

/* Compare two strings
 *
 * Returns: 0 if equal, <0 if s1 < s2, >0 if s1 > s2
 * Why we need this: Command parsing in our shell
 */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/* Compare first n characters of two strings
 *
 * Why we need this: Partial command matching for autocomplete
 */
int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n && *s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/* Fill memory with a constant byte
 *
 * Why we need this: Clearing buffers, initializing structures
 * Implementation: Simple byte-by-byte fill (could be optimized with
 * word-size writes, but this is simple and works)
 */
void *memset(void *dest, int val, size_t len)
{
    unsigned char *ptr = (unsigned char *)dest;
    while (len--)
        *ptr++ = (unsigned char)val;
    return dest;
}

/* Copy memory from src to dest
 *
 * Why we need this: Copying strings, structures, buffers
 * Note: This is a naive implementation and doesn't handle overlapping
 * regions correctly. For production, use memmove() instead.
 */
void *memcpy(void *dest, const void *src, size_t len)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (len--)
        *d++ = *s++;
    return dest;
}

void reverse(char *str)
{
    int i = 0, j = strlen(str) - 1;
    while (i < j)
    {
        char tmp = str[i];
        str[i] = str[j];
        str[j] = tmp;
        i++;
        j--;
    }
}

/* Convert integer to string
 * Essential for printing memory addresses or counter values in your shell
 */
void itoa(int n, char *str)
{
    int i = 0, isNegative = 0;
    if (n == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    if (n < 0)
    {
        isNegative = 1;
        n = -n;
    }
    while (n != 0)
    {
        str[i++] = (n % 10) + '0';
        n = n / 10;
    }
    if (isNegative)
        str[i++] = '-';
    str[i] = '\0';

    /* Reverse the string (you'll need a simple reverse function too!) */
    reverse(str);
}