/*
 * Wine internal Unicode definitions
 *
 * Copyright 2000 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_WINE_UNICODE_H
#define __WINE_WINE_UNICODE_H

#include <stdarg.h>

#include <windef.h>
#include <winbase.h>
#include <winnls.h>
#include <winternl.h>

#include <wine/hostaddrspace_enter.h>

#ifdef __WINE_USE_MSVCRT
#error This file should not be used with msvcrt headers
#endif

#ifndef WINE_UNICODE_INLINE
#define WINE_UNICODE_INLINE static FORCEINLINE
#endif

WINE_UNICODE_INLINE WCHAR tolowerW( WCHAR ch )
{
    return RtlDowncaseUnicodeChar( ch );
}

WINE_UNICODE_INLINE WCHAR toupperW( WCHAR ch )
{
    return RtlUpcaseUnicodeChar( ch );
}

WINE_UNICODE_INLINE int isspaceW( WCHAR wc )
{
#ifndef WINE32ON64_HOSTSTACK
    unsigned short type;
    GetStringTypeW( CT_CTYPE1, &wc, 1, &type );
#else
    /* 32on64 FIXME: This is needed for winecoreaudio.drv/mmdevdrv.c. Maybe find a way to avoid it? */
    unsigned short type;
    if ((ULONGLONG)&wc <= ~0U && (ULONGLONG)&type <= ~0U)
    {
        GetStringTypeW( CT_CTYPE1, ADDRSPACECAST(WCHAR * WIN32PTR, &wc), 1, ADDRSPACECAST(WCHAR * WIN32PTR, &type) );
    }
    else
    {
        unsigned short * WIN32PTR output = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(*output));
        unsigned short * WIN32PTR input = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(*input));
        *input = wc;

        GetStringTypeW( CT_CTYPE1, input, 1, output );
        type = *output;

        RtlFreeHeap(GetProcessHeap(), 0, output);
        RtlFreeHeap(GetProcessHeap(), 0, input);
    }
#endif
    return type & C1_SPACE;
}

WINE_UNICODE_INLINE unsigned int strlenW( const WCHAR * HOSTPTR str )
{
    const WCHAR * HOSTPTR s = str;
    while (*s) s++;
    return s - str;
}

WINE_UNICODE_INLINE WCHAR *strcpyW( WCHAR *dst, const WCHAR * HOSTPTR src )
{
    WCHAR *p = dst;
    while ((*p++ = *src++));
    return dst;
}

WINE_UNICODE_INLINE int strcmpW( const WCHAR * HOSTPTR str1, const WCHAR * HOSTPTR str2 )
{
    while (*str1 && (*str1 == *str2)) { str1++; str2++; }
    return *str1 - *str2;
}

WINE_UNICODE_INLINE int strncmpW( const WCHAR * HOSTPTR str1, const WCHAR * HOSTPTR str2, int n )
{
    if (n <= 0) return 0;
    while ((--n > 0) && *str1 && (*str1 == *str2)) { str1++; str2++; }
    return *str1 - *str2;
}

WINE_UNICODE_INLINE WCHAR *strcatW( WCHAR *dst, const WCHAR * HOSTPTR src )
{
    strcpyW( dst + strlenW(dst), src );
    return dst;
}

WINE_UNICODE_INLINE WCHAR * WIN32PTR strchrW( const WCHAR * WIN32PTR str, WCHAR ch )
{
    do { if (*str == ch) return (WCHAR * WIN32PTR)(ULONG_PTR)str; } while (*str++);
    return NULL;
}

#ifdef __i386_on_x86_64__
WINE_UNICODE_INLINE WCHAR * HOSTPTR strchrW( const WCHAR * HOSTPTR str, WCHAR ch ) __attribute__((overloadable))
{
    do { if (*str == ch) return (WCHAR * HOSTPTR)(ULONGLONG)str; } while (*str++);
    return NULL;
}
#endif

WINE_UNICODE_INLINE WCHAR * WIN32PTR strrchrW( const WCHAR * WIN32PTR str, WCHAR ch )
{
    WCHAR * WIN32PTR ret = NULL;
    do { if (*str == ch) ret = (WCHAR * WIN32PTR)(ULONG_PTR)str; } while (*str++);
    return ret;
}

#ifdef __i386_on_x86_64__
WINE_UNICODE_INLINE WCHAR * HOSTPTR strrchrW( const WCHAR * HOSTPTR str, WCHAR ch ) __attribute__((overloadable))
{
    WCHAR * HOSTPTR ret = NULL;
    do { if (*str == ch) ret = (WCHAR * HOSTPTR)(ULONGLONG)str; } while (*str++);
    return ret;
}
#endif

WINE_UNICODE_INLINE WCHAR * WIN32PTR strpbrkW( const WCHAR * WIN32PTR str, const WCHAR * WIN32PTR accept )
{
    for ( ; *str; str++) if (strchrW( accept, *str )) return (WCHAR * WIN32PTR)(ULONG_PTR)str;
    return NULL;
}

#ifdef __i386_on_x86_64__
WINE_UNICODE_INLINE WCHAR * HOSTPTR strpbrkW( const WCHAR * HOSTPTR str, const WCHAR * HOSTPTR accept ) __attribute__((overloadable))
{
    for ( ; *str; str++) if (strchrW( accept, *str )) return (WCHAR * HOSTPTR)(ULONGLONG)str;
    return NULL;
}
#endif

WINE_UNICODE_INLINE WCHAR *strlwrW( WCHAR *str )
{
    WCHAR *ret;
    for (ret = str; *str; str++) *str = tolowerW(*str);
    return ret;
}

#ifdef __i386_on_x86_64__
WINE_UNICODE_INLINE WCHAR * WIN32PTR strlwrW( WCHAR * WIN32PTR str ) __attribute__((overloadable))
{
    WCHAR * WIN32PTR ret;
    for (ret = str; *str; str++) *str = tolowerW(*str);
    return ret;
}
#endif

WINE_UNICODE_INLINE WCHAR * WIN32PTR memchrW( const WCHAR * WIN32PTR ptr, WCHAR ch, size_t n )
{
    const WCHAR *end;
    for (end = ptr + n; ptr < end; ptr++) if (*ptr == ch) return (WCHAR * WIN32PTR)(ULONG_PTR)ptr;
    return NULL;
}

#ifdef __i386_on_x86_64__
WINE_UNICODE_INLINE WCHAR * HOSTPTR memchrW( const WCHAR * HOSTPTR ptr, WCHAR ch, size_t n ) __attribute__((overloadable))
{
    const WCHAR * HOSTPTR end;
    for (end = ptr + n; ptr < end; ptr++) if (*ptr == ch) return (WCHAR * HOSTPTR)(ULONGLONG)ptr;
    return NULL;
}
#endif

WINE_UNICODE_INLINE int strcmpiW( const WCHAR *str1, const WCHAR *str2 )
{
    for (;;)
    {
        int ret = tolowerW(*str1) - tolowerW(*str2);
        if (ret || !*str1) return ret;
        str1++;
        str2++;
    }
}

WINE_UNICODE_INLINE int strncmpiW( const WCHAR *str1, const WCHAR *str2, int n )
{
    int ret = 0;
    for ( ; n > 0; n--, str1++, str2++)
        if ((ret = tolowerW(*str1) - tolowerW(*str2)) || !*str1) break;
    return ret;
}

WINE_UNICODE_INLINE WCHAR *strstrW( const WCHAR *str, const WCHAR *sub )
{
    while (*str)
    {
        const WCHAR *p1 = str, *p2 = sub;
        while (*p1 && *p2 && *p1 == *p2) { p1++; p2++; }
        if (!*p2) return (WCHAR *)str;
        str++;
    }
    return NULL;
}

#ifdef __i386_on_x86_64__
WINE_UNICODE_INLINE WCHAR * WIN32PTR strstrW( const WCHAR * WIN32PTR str, const WCHAR * HOSTPTR sub ) __attribute__((overloadable))
{
    while (*str)
    {
        const WCHAR * WIN32PTR p1 = str, * HOSTPTR p2 = sub;
        while (*p1 && *p2 && *p1 == *p2) { p1++; p2++; }
        if (!*p2) return (WCHAR * WIN32PTR)str;
        str++;
    }
    return NULL;
}
#endif

WINE_UNICODE_INLINE LONG strtolW( const WCHAR * HOSTPTR s, WCHAR * HOSTPTR * HOSTPTR end, INT base )
{
    BOOL negative = FALSE, empty = TRUE;
    LONG ret = 0;

    if (base < 0 || base == 1 || base > 36) return 0;
    if (end) *end = (WCHAR *)s;
    while (isspaceW(*s)) s++;

    if (*s == '-')
    {
        negative = TRUE;
        s++;
    }
    else if (*s == '+') s++;

    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        base = 16;
        s += 2;
    }
    if (base == 0) base = s[0] != '0' ? 10 : 8;

    while (*s)
    {
        int v;

        if ('0' <= *s && *s <= '9') v = *s - '0';
        else if ('A' <= *s && *s <= 'Z') v = *s - 'A' + 10;
        else if ('a' <= *s && *s <= 'z') v = *s - 'a' + 10;
        else break;
        if (v >= base) break;
        if (negative) v = -v;
        s++;
        empty = FALSE;

        if (!negative && (ret > MAXLONG / base || ret * base > MAXLONG - v))
            ret = MAXLONG;
        else if (negative && (ret < (LONG)MINLONG / base || ret * base < (LONG)(MINLONG - v)))
            ret = MINLONG;
        else
            ret = ret * base + v;
    }

    if (end && !empty) *end = (WCHAR *)s;
    return ret;
}

WINE_UNICODE_INLINE ULONG strtoulW( const WCHAR * HOSTPTR s, WCHAR * HOSTPTR * HOSTPTR end, int base )
{
    BOOL negative = FALSE, empty = TRUE;
    ULONG ret = 0;

    if (base < 0 || base == 1 || base > 36) return 0;
    if (end) *end = (WCHAR *)s;
    while (isspaceW(*s)) s++;

    if (*s == '-')
    {
        negative = TRUE;
        s++;
    }
    else if (*s == '+') s++;

    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        base = 16;
        s += 2;
    }
    if (base == 0) base = s[0] != '0' ? 10 : 8;

    while (*s)
    {
        int v;

        if ('0' <= *s && *s <= '9') v = *s - '0';
        else if ('A' <= *s && *s <= 'Z') v = *s - 'A' + 10;
        else if ('a' <= *s && *s <= 'z') v = *s - 'a' + 10;
        else break;
        if (v >= base) break;
        s++;
        empty = FALSE;

        if (ret > MAXDWORD / base || ret * base > MAXDWORD - v)
            ret = MAXDWORD;
        else
            ret = ret * base + v;
    }

    if (end && !empty) *end = (WCHAR *)s;
    return negative ? -ret : ret;
}

#ifdef __i386_on_x86_64__
WINE_UNICODE_INLINE LONG strtolW( const WCHAR * WIN32PTR nptr, WCHAR * WIN32PTR * WIN32PTR endptr, int base ) __attribute__((overloadable))
{
    WCHAR * HOSTPTR end;
    LONG ret = strtolW( (const WCHAR * HOSTPTR)nptr, &end, base );
    if (endptr) *endptr = ADDRSPACECAST(WCHAR * WIN32PTR, end);
    return ret;
}

WINE_UNICODE_INLINE ULONG strtoulW( const WCHAR * WIN32PTR nptr, LPWSTR * WIN32PTR endptr, INT base ) __attribute__((overloadable))
{
    WCHAR * HOSTPTR end;
    ULONG ret = strtoulW( (const WCHAR * HOSTPTR)nptr, &end, base );
    if (endptr) *endptr = ADDRSPACECAST(WCHAR * WIN32PTR, end);
    return ret;
}
#endif

WINE_UNICODE_INLINE long int atolW( const WCHAR * HOSTPTR str )
{
    return strtolW( str, 0, 10 );
}

WINE_UNICODE_INLINE int atoiW( const WCHAR * HOSTPTR str )
{
    return (int)strtolW( str, (WCHAR **)0, 10 );
}

#include "wine/hostaddrspace_exit.h"

#ifdef __i386_on_x86_64__
NTSYSAPI int __cdecl _vsnwprintf(WCHAR*,uint32_t,const WCHAR*,__ms_va_list);
#else
NTSYSAPI int __cdecl _vsnwprintf(WCHAR*,size_t,const WCHAR*,__ms_va_list);
#endif

static inline int WINAPIV snprintfW( WCHAR *str, size_t len, const WCHAR *format, ...)
{
    int retval;
    __ms_va_list valist;
    __ms_va_start(valist, format);
    retval = _vsnwprintf(str, len, format, valist);
    __ms_va_end(valist);
    return retval;
}

static inline int WINAPIV sprintfW( WCHAR *str, const WCHAR *format, ...)
{
    int retval;
    __ms_va_list valist;
    __ms_va_start(valist, format);
    retval = _vsnwprintf(str, MAXLONG, format, valist);
    __ms_va_end(valist);
    return retval;
}

#undef WINE_UNICODE_INLINE

#endif  /* __WINE_WINE_UNICODE_H */
