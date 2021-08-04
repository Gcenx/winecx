/*
 * Memory definitions
 *
 * Derived from the mingw header written by Colin Peters.
 * Modified for Wine use by Jon Griffiths and Francois Gouget.
 * This file is in the public domain.
 */
#ifndef __WINE_MEMORY_H
#define __WINE_MEMORY_H

#include "wine/winheader_enter.h"

#include <corecrt.h>
#include <wine/asm.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _CRT_MEMORY_DEFINED
#define _CRT_MEMORY_DEFINED

_ACRTIMP void*   __cdecl memchr(const void*,int,size_t);
_ACRTIMP int     __cdecl memcmp(const void*,const void*,size_t);
_ACRTIMP void*   __cdecl memcpy(void*,const void*,size_t);

#ifdef __i386_on_x86_64__
/* copypasted from msvcrt/string.c */
#ifdef WORDS_BIGENDIAN
# define MERGE(w1, sh1, w2, sh2) ((w1 << sh1) | (w2 >> sh2))
#else
# define MERGE(w1, sh1, w2, sh2) ((w1 >> sh1) | (w2 << sh2))
#endif
static inline void* HOSTPTR __cdecl memcpy(void * HOSTPTR dst, const void * HOSTPTR src, MSVCRT_size_t n) __attribute__((overloadable))
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    int sh1;

    if (!n) return dst;

    if ((size_t)dst - (size_t)src >= n)
    {
        for (; (size_t)d % sizeof(size_t) && n; n--) *d++ = *s++;

        sh1 = 8 * ((size_t)s % sizeof(size_t));
        if (!sh1)
        {
            while (n >= sizeof(size_t))
            {
                *(size_t*)d = *(size_t*)s;
                s += sizeof(size_t);
                d += sizeof(size_t);
                n -= sizeof(size_t);
            }
        }
        else if (n >= 2 * sizeof(size_t))
        {
            int sh2 = 8 * sizeof(size_t) - sh1;
            size_t x, y;

            s -= sh1 / 8;
            x = *(size_t*)s;
            do
            {
                s += sizeof(size_t);
                y = *(size_t*)s;
                *(size_t*)d = MERGE(x, sh1, y, sh2);
                d += sizeof(size_t);

                s += sizeof(size_t);
                x = *(size_t*)s;
                *(size_t*)d = MERGE(y, sh1, x, sh2);
                d += sizeof(size_t);

                n -= 2 * sizeof(size_t);
            } while (n >= 2 * sizeof(size_t));
            s += sh1 / 8;
        }
        while (n--) *d++ = *s++;
        return dst;
    }
    else
    {
        d += n;
        s += n;

        for (; (size_t)d % sizeof(size_t) && n; n--) *--d = *--s;

        sh1 = 8 * ((size_t)s % sizeof(size_t));
        if (!sh1)
        {
            while (n >= sizeof(size_t))
            {
                s -= sizeof(size_t);
                d -= sizeof(size_t);
                *(size_t*)d = *(size_t*)s;
                n -= sizeof(size_t);
            }
        }
        else if (n >= 2 * sizeof(size_t))
        {
            int sh2 = 8 * sizeof(size_t) - sh1;
            size_t x, y;

            s -= sh1 / 8;
            x = *(size_t*)s;
            do
            {
                s -= sizeof(size_t);
                y = *(size_t*)s;
                d -= sizeof(size_t);
                *(size_t*)d = MERGE(y, sh1, x, sh2);

                s -= sizeof(size_t);
                x = *(size_t*)s;
                d -= sizeof(size_t);
                *(size_t*)d = MERGE(x, sh1, y, sh2);

                n -= 2 * sizeof(size_t);
            } while (n >= 2 * sizeof(size_t));
            s += sh1 / 8;
        }
        while (n--) *--d = *--s;
    }
    return dst;
}
#undef MERGE
#endif

_ACRTIMP errno_t __cdecl memcpy_s(void*,size_t,const void*,size_t);
_ACRTIMP void*   __cdecl memset(void*,int,size_t);
_ACRTIMP void*   __cdecl _memccpy(void*,const void*,int,size_t);
_ACRTIMP int     __cdecl _memicmp(const void*,const void*,size_t);
_ACRTIMP int     __cdecl _memicmp_l(const void*,const void*,size_t,_locale_t);

static inline int memicmp(const void* s1, const void* s2, size_t len) { return _memicmp(s1, s2, len); }
static inline void* memccpy(void *s1, const void *s2, int c, size_t n) { return _memccpy(s1, s2, c, n); }

#endif /* _CRT_MEMORY_DEFINED */

#ifdef __cplusplus
}
#endif

#include "wine/winheader_exit.h"

#endif /* __WINE_MEMORY_H */
