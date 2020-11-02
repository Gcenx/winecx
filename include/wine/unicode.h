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
#include <wine/hostaddrspace_enter.h>
#include <wine/asm.h>

#ifdef __WINE_WINE_TEST_H
#error This file should not be used in Wine tests
#endif

#ifdef __WINE_USE_MSVCRT
#error This file should not be used with msvcrt headers
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINE_UNICODE_INLINE
#define WINE_UNICODE_INLINE static inline
#endif

/* code page info common to SBCS and DBCS */
struct cp_info
{
    unsigned int          codepage;          /* codepage id */
    unsigned int          char_size;         /* char size (1 or 2 bytes) */
    WCHAR                 def_char;          /* default char value (can be double-byte) */
    WCHAR                 def_unicode_char;  /* default Unicode char value */
    const char           *name;              /* code page name */
};

struct sbcs_table
{
    struct cp_info        info;
    const WCHAR * HOSTPTR cp2uni;            /* code page -> Unicode map */
    const WCHAR * HOSTPTR cp2uni_glyphs;     /* code page -> Unicode map with glyph chars */
    const unsigned char  *uni2cp_low;        /* Unicode -> code page map */
    const unsigned short *uni2cp_high;
};

struct dbcs_table
{
    struct cp_info        info;
    const WCHAR * HOSTPTR cp2uni;            /* code page -> Unicode map */
    const unsigned char  *cp2uni_leadbytes;
    const unsigned short *uni2cp_low;        /* Unicode -> code page map */
    const unsigned short *uni2cp_high;
    unsigned char         lead_bytes[12];    /* lead bytes ranges */
};

union cptable
{
    struct cp_info    info;
    struct sbcs_table sbcs;
    struct dbcs_table dbcs;
};

extern const union cptable *wine_cp_get_table( unsigned int codepage );
extern const union cptable *wine_cp_enum_table( unsigned int index );

extern int wine_cp_mbstowcs( const union cptable *table, int flags,
                             const char *src, int srclen,
                             WCHAR * HOSTPTR dst, int dstlen );
extern int wine_cp_wcstombs( const union cptable *table, int flags,
                             const WCHAR * HOSTPTR src, int srclen,
                             char *dst, int dstlen, const char *defchar, int *used );
extern int wine_cpsymbol_mbstowcs( const char *src, int srclen, WCHAR * HOSTPTR dst, int dstlen );
extern int wine_cpsymbol_wcstombs( const WCHAR * HOSTPTR src, int srclen, char *dst, int dstlen );
extern int wine_utf8_mbstowcs( int flags, const char *src, int srclen, WCHAR * HOSTPTR dst, int dstlen );
extern int wine_utf8_wcstombs( int flags, const WCHAR * HOSTPTR src, int srclen, char *dst, int dstlen );

extern int wine_compare_string( int flags, const WCHAR * HOSTPTR str1, int len1, const WCHAR * HOSTPTR str2, int len2 );
extern int wine_get_sortkey( int flags, const WCHAR * HOSTPTR src, int srclen, char *dst, int dstlen );
extern int wine_fold_string( int flags, const WCHAR * HOSTPTR src, int srclen , WCHAR * HOSTPTR dst, int dstlen );

extern unsigned int wine_compose_string( WCHAR * HOSTPTR str, unsigned int len );
extern unsigned int wine_decompose_string( int flags, const WCHAR * HOSTPTR src, unsigned int srclen, WCHAR * HOSTPTR dst, unsigned int dstlen );
#define WINE_DECOMPOSE_COMPAT     1
#define WINE_DECOMPOSE_REORDER    2

extern int strcmpiW( const WCHAR * HOSTPTR str1, const WCHAR * HOSTPTR str2 );
extern int strncmpiW( const WCHAR * HOSTPTR str1, const WCHAR * HOSTPTR str2, int n );
extern int memicmpW( const WCHAR * HOSTPTR str1, const WCHAR * HOSTPTR str2, int n );
extern WCHAR * HOSTPTR strstrW( const WCHAR * HOSTPTR str, const WCHAR * HOSTPTR sub );
extern long int strtolW( const WCHAR * HOSTPTR nptr, WCHAR * HOSTPTR * HOSTPTR endptr, int base );
extern unsigned long int strtoulW( const WCHAR * HOSTPTR nptr, WCHAR * HOSTPTR * HOSTPTR endptr, int base );
extern int sprintfW( WCHAR * HOSTPTR str, const WCHAR * HOSTPTR format, ... );
extern int snprintfW( WCHAR * HOSTPTR str, size_t len, const WCHAR * HOSTPTR format, ... );
extern int vsprintfW( WCHAR * HOSTPTR str, const WCHAR * HOSTPTR format, va_list valist );
extern int vsnprintfW( WCHAR * HOSTPTR str, size_t len, const WCHAR * HOSTPTR format, va_list valist );

#ifdef __i386_on_x86_64__
extern WCHAR * WIN32PTR strstrW( const WCHAR * WIN32PTR str, const WCHAR * HOSTPTR sub ) __attribute__((overloadable)) asm(__ASM_NAME("strstrW_WIN32PTR"));
extern long int strtolW( const WCHAR * WIN32PTR nptr, WCHAR * WIN32PTR * WIN32PTR endptr, int base ) __attribute__((overloadable)) asm(__ASM_NAME("strtolW_WIN32PTR"));
extern unsigned long int strtoulW( const WCHAR * WIN32PTR nptr, WCHAR * WIN32PTR * WIN32PTR endptr, int base ) __attribute__((overloadable)) asm(__ASM_NAME("strtoulW_WIN32PTR"));
#endif

WINE_UNICODE_INLINE int wine_is_dbcs_leadbyte( const union cptable *table, unsigned char ch )
{
    return (table->info.char_size == 2) && (table->dbcs.cp2uni_leadbytes[ch]);
}

WINE_UNICODE_INLINE WCHAR tolowerW( WCHAR ch )
{
    extern const WCHAR wine_casemap_lower[];
    return ch + wine_casemap_lower[wine_casemap_lower[ch >> 8] + (ch & 0xff)];
}

WINE_UNICODE_INLINE WCHAR toupperW( WCHAR ch )
{
    extern const WCHAR wine_casemap_upper[];
    return ch + wine_casemap_upper[wine_casemap_upper[ch >> 8] + (ch & 0xff)];
}

/* the character type contains the C1_* flags in the low 12 bits */
/* and the C2_* type in the high 4 bits */
WINE_UNICODE_INLINE unsigned short get_char_typeW( WCHAR ch )
{
    extern const unsigned short wine_wctype_table[];
    return wine_wctype_table[wine_wctype_table[ch >> 8] + (ch & 0xff)];
}

WINE_UNICODE_INLINE int iscntrlW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_CNTRL;
}

WINE_UNICODE_INLINE int ispunctW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_PUNCT;
}

WINE_UNICODE_INLINE int isspaceW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_SPACE;
}

WINE_UNICODE_INLINE int isdigitW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_DIGIT;
}

WINE_UNICODE_INLINE int isxdigitW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_XDIGIT;
}

WINE_UNICODE_INLINE int islowerW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_LOWER;
}

WINE_UNICODE_INLINE int isupperW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_UPPER;
}

WINE_UNICODE_INLINE int isalnumW( WCHAR wc )
{
    return get_char_typeW(wc) & (C1_ALPHA|C1_DIGIT|C1_LOWER|C1_UPPER);
}

WINE_UNICODE_INLINE int isalphaW( WCHAR wc )
{
    return get_char_typeW(wc) & (C1_ALPHA|C1_LOWER|C1_UPPER);
}

WINE_UNICODE_INLINE int isgraphW( WCHAR wc )
{
    return get_char_typeW(wc) & (C1_ALPHA|C1_PUNCT|C1_DIGIT|C1_LOWER|C1_UPPER);
}

WINE_UNICODE_INLINE int isprintW( WCHAR wc )
{
    return get_char_typeW(wc) & (C1_ALPHA|C1_BLANK|C1_PUNCT|C1_DIGIT|C1_LOWER|C1_UPPER);
}

/* some useful string manipulation routines */

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

/* strncpy doesn't do what you think, don't use it */
#define strncpyW(d,s,n) error do_not_use_strncpyW_use_lstrcpynW_or_memcpy_instead

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

WINE_UNICODE_INLINE size_t strspnW( const WCHAR * HOSTPTR str, const WCHAR *accept )
{
    const WCHAR * HOSTPTR ptr;
    for (ptr = str; *ptr; ptr++) if (!strchrW( accept, *ptr )) break;
    return ptr - str;
}

WINE_UNICODE_INLINE size_t strcspnW( const WCHAR * HOSTPTR str, const WCHAR *reject )
{
    const WCHAR * HOSTPTR ptr;
    for (ptr = str; *ptr; ptr++) if (strchrW( reject, *ptr )) break;
    return ptr - str;
}

WINE_UNICODE_INLINE WCHAR * WIN32PTR strlwrW( WCHAR * WIN32PTR str )
{
    WCHAR * WIN32PTR ret;
    for (ret = str; *str; str++) *str = tolowerW(*str);
    return ret;
}

WINE_UNICODE_INLINE WCHAR *struprW( WCHAR *str )
{
    WCHAR *ret;
    for (ret = str; *str; str++) *str = toupperW(*str);
    return ret;
}

WINE_UNICODE_INLINE WCHAR * WIN32PTR memchrW( const WCHAR * WIN32PTR ptr, WCHAR ch, size_t n )
{
    const WCHAR * WIN32PTR end;
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

WINE_UNICODE_INLINE WCHAR * WIN32PTR memrchrW( const WCHAR * WIN32PTR ptr, WCHAR ch, size_t n )
{
    const WCHAR * WIN32PTR end;
    WCHAR * WIN32PTR ret = NULL;
    for (end = ptr + n; ptr < end; ptr++) if (*ptr == ch) ret = (WCHAR * WIN32PTR)(ULONG_PTR)ptr;
    return ret;
}

#ifdef __i386_on_x86_64__
WINE_UNICODE_INLINE WCHAR * HOSTPTR memrchrW( const WCHAR * HOSTPTR ptr, WCHAR ch, size_t n ) __attribute__((overloadable))
{
    const WCHAR * HOSTPTR end;
    WCHAR * HOSTPTR ret = NULL;
    for (end = ptr + n; ptr < end; ptr++) if (*ptr == ch) ret = (WCHAR * HOSTPTR)(ULONGLONG)ptr;
    return ret;
}
#endif

WINE_UNICODE_INLINE long int atolW( const WCHAR * HOSTPTR str )
{
    return strtolW( str, 0, 10 );
}

WINE_UNICODE_INLINE int atoiW( const WCHAR * HOSTPTR str )
{
    return (int)atolW( str );
}

#undef WINE_UNICODE_INLINE

#ifdef __cplusplus
}
#endif

#include "wine/hostaddrspace_exit.h"

#endif  /* __WINE_WINE_UNICODE_H */
