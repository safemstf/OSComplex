/* string.c - Basic string manipulation functions
 *
 * Since we're writing an OS, we can't use libc. We need to implement
 * our own string functions. These are simplified versions of the
 * standard C library functions.
 */

#include "../kernel/kernel.h"

/* ==================== EXISTING FUNCTIONS ==================== */

/* Calculate length of a null-terminated string */
size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

/* Calculate length up to n characters */
size_t strnlen(const char *str, size_t maxlen)
{
    size_t len = 0;
    while (len < maxlen && str[len])
        len++;
    return len;
}

/* Compare two strings */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/* Compare first n characters of two strings */
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

/* Fill memory with a constant byte */
void *memset(void *dest, int val, size_t len)
{
    unsigned char *ptr = (unsigned char *)dest;
    while (len--)
        *ptr++ = (unsigned char)val;
    return dest;
}

/* Copy memory from src to dest */
void *memcpy(void *dest, const void *src, size_t len)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (len--)
        *d++ = *s++;
    return dest;
}

/* Copy memory handling overlapping regions */
void *memmove(void *dest, const void *src, size_t len)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    if (d < s) {
        /* Copy forward */
        while (len--)
            *d++ = *s++;
    } else {
        /* Copy backward */
        d += len;
        s += len;
        while (len--)
            *(--d) = *(--s);
    }
    return dest;
}

/* Compare two memory regions */
int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2)
            return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

/* Find character in string */
char *strchr(const char *s, int c)
{
    while (*s != (char)c)
        if (!*s++)
            return NULL;
    return (char *)s;
}

/* Find LAST occurrence of character in string
 * 
 * This is needed by VFS for finding the last '/' in paths
 * Example: "/path/to/file.txt" -> returns pointer to last '/'
 */
char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    
    /* Scan entire string, remember last occurrence */
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    
    /* Check if we're looking for null terminator */
    if ((char)c == '\0') {
        return (char *)s;
    }
    
    return (char *)last;
}

/* Find character in memory */
void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    while (n--) {
        if (*p == (unsigned char)c)
            return (void *)p;
        p++;
    }
    return NULL;
}

/* Copy string */
char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

/* Copy string with length limit */
char *strncpy(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (n && (*d++ = *src++))
        n--;
    while (n--)
        *d++ = '\0';
    return dest;
}

/* Concatenate strings */
char *strcat(char *dest, const char *src)
{
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

/* Concatenate strings with length limit */
char *strncat(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (*d) d++;
    while (n-- && (*d++ = *src++));
    *d = '\0';
    return dest;
}

static void reverse(char *str)
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

/* Convert integer to string */
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
    reverse(str);
}

/* Convert unsigned integer to string */
void utoa(uint32_t n, char *str, int base)
{
    int i = 0;
    if (n == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    
    while (n != 0) {
        int rem = n % base;
        str[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        n = n / base;
    }
    str[i] = '\0';
    reverse(str);
}

/* ==================== HEAP-ENABLED FUNCTIONS ==================== */

/* Duplicate string (uses heap) */
char *strdup(const char *s)
{
    if (!s) return NULL;
    
    size_t len = strlen(s) + 1;
    char *new = kmalloc(len);
    if (new)
        memcpy(new, s, len);
    return new;
}

/* Duplicate string with length limit (uses heap) */
char *strndup(const char *s, size_t n)
{
    if (!s) return NULL;
    
    size_t len = strnlen(s, n);
    char *new = kmalloc(len + 1);
    if (new) {
        memcpy(new, s, len);
        new[len] = '\0';
    }
    return new;
}

/* Concatenate multiple strings (uses heap) */
char *strconcat(const char *s1, const char *s2)
{
    if (!s1 && !s2) return NULL;
    if (!s1) return strdup(s2);
    if (!s2) return strdup(s1);
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    char *new = kmalloc(len1 + len2 + 1);
    if (new) {
        memcpy(new, s1, len1);
        memcpy(new + len1, s2, len2 + 1);
    }
    return new;
}

/* Format string (simple version, uses heap) */
char *strfmt(const char *fmt, ...)
{
    /* Simple buffer for now - we'll implement proper formatting later */
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    
    /* For now, just copy the format string */
    strncpy(buffer, fmt, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    va_end(args);
    
    return strdup(buffer);
}

/* Tokenize string (strtok replacement) */
char *strtok_r(char *str, const char *delim, char **saveptr)
{
    if (!delim) return NULL;
    
    if (str) {
        *saveptr = str;
    } else if (!*saveptr) {
        return NULL;
    }
    
    /* Skip leading delimiters */
    char *start = *saveptr;
    while (*start && strchr(delim, *start)) {
        start++;
    }
    
    if (!*start) {
        *saveptr = start;
        return NULL;
    }
    
    /* Find end of token */
    char *end = start;
    while (*end && !strchr(delim, *end)) {
        end++;
    }
    
    if (*end) {
        *end = '\0';
        *saveptr = end + 1;
    } else {
        *saveptr = end;
    }
    
    return start;
}

/* Case-insensitive string comparison */
int stricmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        char c1 = *s1++;
        char c2 = *s2++;
        
        /* Convert to lowercase for comparison */
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        
        if (c1 != c2)
            return c1 - c2;
    }
    return *s1 - *s2;
}

/* Trim whitespace from string (in-place) */
char *strtrim(char *str)
{
    if (!str) return NULL;
    
    /* Trim leading spaces */
    char *end;
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')
        str++;
    
    /* Trim trailing spaces */
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
        end--;
    
    *(end + 1) = '\0';
    return str;
}

/* Find substring in string */
char *strstr(const char *haystack, const char *needle)
{
    if (!*needle) return (char *)haystack;
    
    for (const char *h = haystack; *h; h++) {
        const char *n = needle;
        const char *h2 = h;
        
        while (*h2 && *n && *h2 == *n) {
            h2++;
            n++;
        }
        
        if (!*n) return (char *)h;  /* Found */
    }
    
    return NULL;
}