#include <string.h>

char *strchr(const char *s, int c)
{
    const char ch = c;
    while (*s != ch)
    {
        if (*s == 0)
            return NULL;
        s++;
    }
    return (char *)s;
}

size_t strlen(const char *s)
{
    const char *sc = s;
    while (*sc)
        sc++;
    return sc - s;
}
void *memcpy(void *s1, const void *s2, size_t n)
{
    unsigned char *su1 = (unsigned char *)s1;
    const unsigned char *su2 = (const unsigned char *)s2;
    while (n > 0)
    {
        *su1++ = *su2++;
        n--;
    }
    return (void *)s1;
}
