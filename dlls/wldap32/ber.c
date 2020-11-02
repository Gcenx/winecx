/*
 * WLDAP32 - LDAP support for Wine
 *
 * Copyright 2005 Hans Leidekker
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

#include "config.h"

#include <stdarg.h>
#ifdef HAVE_LDAP_H
#include <ldap.h>
#endif

#include "windef.h"
#include "winbase.h"

#include "winldap_private.h"
#include "wldap32.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wldap32);

#ifndef LBER_ERROR
# define LBER_ERROR (~0U)
#endif

/***********************************************************************
 *      ber_alloc_t     (WLDAP32.@)
 *
 * Allocate a berelement structure.
 *
 * PARAMS
 *  options [I] Must be LBER_USE_DER.
 *
 * RETURNS
 *  Success: Pointer to an allocated berelement structure.
 *  Failure: NULL
 *
 * NOTES
 *  Free the berelement structure with ber_free.
 */
WLDAP32_BerElement * CDECL WLDAP32_ber_alloc_t( INT options )
{
#ifdef HAVE_LDAP
    return ber_wrap( ber_alloc_t( options ), 1 );
#else
    return NULL;
#endif
}


/***********************************************************************
 *      ber_bvdup     (WLDAP32.@)
 *
 * Copy a berval structure.
 *
 * PARAMS
 *  berval [I] Pointer to the berval structure to be copied.
 *
 * RETURNS
 *  Success: Pointer to a copy of the berval structure.
 *  Failure: NULL
 *
 * NOTES
 *  Free the copy with ber_bvfree.
 */
BERVAL * CDECL WLDAP32_ber_bvdup( BERVAL *berval )
{
#ifdef HAVE_LDAP
    return bvwdup( berval );
#else
    return NULL;
#endif
}


/***********************************************************************
 *      ber_bvecfree     (WLDAP32.@)
 *
 * Free an array of berval structures.
 *
 * PARAMS
 *  berval [I] Pointer to an array of berval structures.
 *
 * RETURNS
 *  Nothing.
 *
 * NOTES
 *  Use this function only to free an array of berval structures
 *  returned by a call to ber_scanf with a 'V' in the format string.
 */
void CDECL WLDAP32_ber_bvecfree( PBERVAL *berval )
{
#ifdef HAVE_LDAP
    bvecfree( berval );
#endif
}


/***********************************************************************
 *      ber_bvfree     (WLDAP32.@)
 *
 * Free a berval structure.
 *
 * PARAMS
 *  berval [I] Pointer to a berval structure.
 *
 * RETURNS
 *  Nothing.
 *
 * NOTES
 *  Use this function only to free berval structures allocated by
 *  an LDAP API.
 */
void CDECL WLDAP32_ber_bvfree( BERVAL *berval )
{
#ifdef HAVE_LDAP
    bvfree( berval );
#endif
}


/***********************************************************************
 *      ber_first_element     (WLDAP32.@)
 *
 * Return the tag of the first element in a set or sequence.
 *
 * PARAMS
 *  berelement [I] Pointer to a berelement structure.
 *  len        [O] Receives the length of the first element.
 *  opaque     [O] Receives a pointer to a cookie.
 *
 * RETURNS
 *  Success: Tag of the first element.
 *  Failure: LBER_DEFAULT (no more data).
 *
 * NOTES
 *  len and cookie should be passed to ber_next_element.
 */
ULONG CDECL WLDAP32_ber_first_element( WLDAP32_BerElement *berelement, ULONG *len, CHAR **opaque )
{
#ifdef HAVE_LDAP
    char * HOSTPTR cookie;
    ber_len_t bvlen;
    ber_tag_t ret = ber_first_element( ber_get( berelement ), &bvlen, &cookie );
    *len = (ULONG)bvlen;
    return ber_cookie_wrap( ret, cookie, opaque);
#else
    return LBER_ERROR;
#endif
}


/***********************************************************************
 *      ber_flatten     (WLDAP32.@)
 *
 * Flatten a berelement structure into a berval structure.
 *
 * PARAMS
 *  berelement [I] Pointer to a berelement structure.
 *  berval    [O] Pointer to a berval structure.
 *
 * RETURNS
 *  Success: 0
 *  Failure: LBER_ERROR
 *
 * NOTES
 *  Free the berval structure with ber_bvfree.
 */
INT CDECL WLDAP32_ber_flatten( WLDAP32_BerElement *berelement, PBERVAL *berval )
{
#ifdef HAVE_LDAP
    struct berval *bvret = NULL;
    INT ret = ber_flatten( ber_get( berelement ), &bvret );
    ret = bvconvert_and_free( ret, bvret, berval );
    return ret;
#else
    return LBER_ERROR;
#endif
}


/***********************************************************************
 *      ber_free     (WLDAP32.@)
 *
 * Free a berelement structure.
 *
 * PARAMS
 *  berelement [I] Pointer to the berelement structure to be freed.
 *  buf       [I] Flag.
 *
 * RETURNS
 *  Nothing.
 *
 * NOTES
 *  Set buf to 0 if the berelement was allocated with ldap_first_attribute
 *  or ldap_next_attribute, otherwise set it to 1.
 */
void CDECL WLDAP32_ber_free( WLDAP32_BerElement *berelement, INT buf )
{
#ifdef HAVE_LDAP
    ber_unwrap_and_free( berelement, buf );
#endif
}


/***********************************************************************
 *      ber_init     (WLDAP32.@)
 *
 * Initialise a berelement structure from a berval structure.
 *
 * PARAMS
 *  berval [I] Pointer to a berval structure.
 *
 * RETURNS
 *  Success: Pointer to a berelement structure.
 *  Failure: NULL
 *
 * NOTES
 *  Call ber_free to free the returned berelement structure.
 */
WLDAP32_BerElement * CDECL WLDAP32_ber_init( BERVAL *berval )
{
#ifdef HAVE_LDAP
    struct berval bvtmp;

    bvtmp.bv_len = berval->bv_len;
    bvtmp.bv_val = berval->bv_val;

    return ber_wrap( ber_init( &bvtmp ), 1 );
#else
    return NULL;
#endif
}


/***********************************************************************
 *      ber_next_element     (WLDAP32.@)
 *
 * Return the tag of the next element in a set or sequence.
 *
 * PARAMS
 *  berelement [I]   Pointer to a berelement structure.
 *  len        [I/O] Receives the length of the next element.
 *  opaque     [I/O] Pointer to a cookie.
 *
 * RETURNS
 *  Success: Tag of the next element.
 *  Failure: LBER_DEFAULT (no more data).
 *
 * NOTES
 *  len and cookie are initialized by ber_first_element and should
 *  be passed on in subsequent calls to ber_next_element.
 */
ULONG CDECL WLDAP32_ber_next_element( WLDAP32_BerElement *berelement, ULONG *len, CHAR *opaque )
{
#ifdef HAVE_LDAP
    ber_len_t bvlen;
    ber_tag_t ret = ber_next_element( ber_get( berelement ), &bvlen, ber_cookie_get( opaque ) );
    *len = bvlen;
    /* The only way to free a wrapped cookie is to handle failure of ber_next_element(). */
    if (ret == LBER_DEFAULT) ber_cookie_free( opaque );
    return ret;
#else
    return LBER_ERROR;
#endif
}


/***********************************************************************
 *      ber_peek_tag     (WLDAP32.@)
 *
 * Return the tag of the next element.
 *
 * PARAMS
 *  berelement [I] Pointer to a berelement structure.
 *  len        [O] Receives the length of the next element.
 *
 * RETURNS
 *  Success: Tag of the next element.
 *  Failure: LBER_DEFAULT (no more data).
 */
ULONG CDECL WLDAP32_ber_peek_tag( WLDAP32_BerElement *berelement, ULONG *len )
{
#ifdef HAVE_LDAP
    ber_len_t bvlen;
    ber_tag_t ret = ber_peek_tag( ber_get( berelement ), &bvlen );
    *len = (ULONG)bvlen;
    return ret;
#else
    return LBER_ERROR;
#endif
}


/***********************************************************************
 *      ber_skip_tag     (WLDAP32.@)
 *
 * Skip the current tag and return the tag of the next element.
 *
 * PARAMS
 *  berelement [I] Pointer to a berelement structure.
 *  len        [O] Receives the length of the skipped element.
 *
 * RETURNS
 *  Success: Tag of the next element.
 *  Failure: LBER_DEFAULT (no more data).
 */
ULONG CDECL WLDAP32_ber_skip_tag( WLDAP32_BerElement *berelement, ULONG *len )
{
#ifdef HAVE_LDAP
    ber_len_t bvlen;
    ber_tag_t ret = ber_skip_tag( ber_get( berelement ), &bvlen );
    *len = (ULONG)bvlen;
    return ret;
#else
    return LBER_ERROR;
#endif
}


/***********************************************************************
 *      ber_printf     (WLDAP32.@)
 *
 * Encode a berelement structure.
 *
 * PARAMS
 *  berelement [I/O] Pointer to a berelement structure.
 *  fmt        [I]   Format string.
 *  ...        [I]   Values to encode.
 *
 * RETURNS
 *  Success: Non-negative number. 
 *  Failure: LBER_ERROR
 *
 * NOTES
 *  berelement must have been allocated with ber_alloc_t. This function
 *  can be called multiple times to append data.
 *  No address space check occurs for pointer arguments to ber_printf(),
 *  which expects a 64-bit pointer and 64-bit nested pointers.
 */
INT WINAPIV WLDAP32_ber_printf( WLDAP32_BerElement *ber, PCHAR fmt, ... )
{
#ifdef HAVE_LDAP
    BerElement *berelement = ber_get(ber);
    __ms_va_list list;
    ber_tag_t ret = 0;
    char new_fmt[2];

    new_fmt[1] = 0;
    __ms_va_start( list, fmt );
    while (*fmt)
    {
        new_fmt[0] = *fmt++;
        switch(new_fmt[0])
        {
        case 'b':
        case 'e':
        case 'i':
            {
                ber_int_t i = va_arg( list, int );
                ret = ber_printf( berelement, new_fmt, i );
                break;
            }
        case 'o':
        case 's':
            {
                /* Pass a 64-bit pointer. The type is unchecked. */
                char * HOSTPTR str = va_arg( list, char * );
                ret = ber_printf( berelement, new_fmt, str );
                break;
            }
        case 't':
            {
                ber_tag_t tag = va_arg( list, ULONG );
                ret = ber_printf( berelement, new_fmt, tag );
                break;
            }
        case 'v':
            {
                char **array = va_arg( list, char ** );
                char * HOSTPTR * HOSTPTR hostarray = strhostarray( array );
                ret = LBER_ERROR;
                if (hostarray)
                {
                    ret = ber_printf( berelement, new_fmt, hostarray );
                    strhostarrayfree( hostarray );
                }
                break;
            }
        case 'V':
            {
                BERVAL **array = va_arg( list, BERVAL ** );
                struct berval ** HOSTPTR bvarray = bvarrayconvert( array );
                ret = LBER_ERROR;
                if (bvarray)
                {
                    ret = ber_printf( berelement, new_fmt, bvarray );
                    bvtemparrayfree( bvarray );
                }
                break;
            }
        case 'X':
            {
                char * HOSTPTR str = va_arg( list, char * );
                ber_len_t len = va_arg( list, ULONG );
                new_fmt[0] = 'B';  /* 'X' is deprecated */
                ret = ber_printf( berelement, new_fmt, str, len );
                break;
            }
        case 'n':
        case '{':
        case '}':
        case '[':
        case ']':
            ret = ber_printf( berelement, new_fmt );
            break;
        default:
            FIXME( "Unknown format '%c'\n", new_fmt[0] );
            ret = -1;
            break;
        }
        if (ret == -1) break;
    }
    __ms_va_end( list );
    return ret;
#else
    return LBER_ERROR;
#endif
}


/***********************************************************************
 *      ber_scanf     (WLDAP32.@)
 *
 * Decode a berelement structure.
 *
 * PARAMS
 *  berelement [I/O] Pointer to a berelement structure.
 *  fmt        [I]   Format string.
 *  ...        [I]   Pointers to values to be decoded.
 *
 * RETURNS
 *  Success: Non-negative number. 
 *  Failure: LBER_ERROR
 *
 * NOTES
 *  berelement must have been allocated with ber_init. This function
 *  can be called multiple times to decode data.
 *  No address space check occurs for pointer arguments to ber_scanf(),
 *  which expects a 64-bit pointer and 64-bit nested pointers.
 */
INT WINAPIV WLDAP32_ber_scanf( WLDAP32_BerElement *ber, PCHAR fmt, ... )
{
#ifdef HAVE_LDAP
    BerElement *berelement = ber_get(ber);
    __ms_va_list list;
    ber_tag_t ret = 0;
    char new_fmt[2];

    new_fmt[1] = 0;
    __ms_va_start( list, fmt );
    while (*fmt)
    {
        new_fmt[0] = *fmt++;
        switch(new_fmt[0])
        {
        case 'a':
            {
                char **ptr = va_arg( list, char ** );
                char * HOSTPTR str = NULL;
                /* Pass a 64-bit pointer. The type is unchecked. */
                ret = ber_scanf( berelement, new_fmt, (void * HOSTPTR)&str );
                if (!(*ptr = berstrdup_and_free( str ))) ret = LBER_ERROR;
                break;
            }
        case 'b':
        case 'e':
        case 'i':
            {
                int * HOSTPTR i = va_arg( list, int * );
                ret = ber_scanf( berelement, new_fmt, i );
                break;
            }
        case 't':
            {
                ULONG *tag = va_arg( list, ULONG * );
                ber_tag_t ber_tag;
                ret = ber_scanf( berelement, new_fmt, (ber_tag_t * HOSTPTR)&ber_tag );
                *tag = ber_tag;
                break;
            }
        case 'v':
            {
                char ***array = va_arg( list, char *** );
                char * HOSTPTR * HOSTPTR ptr = NULL;
                ret = ber_scanf( berelement, new_fmt, (void * HOSTPTR)&ptr );
                if (!(*array = berstrarraydup_and_free( ptr ))) ret = LBER_ERROR;
                break;
            }
        case 'B':
            {
                char **ptr = va_arg( list, char ** );
                char * HOSTPTR str = NULL;
                ULONG *len = va_arg( list, ULONG * );
                ber_len_t ber_len;
                ret = ber_scanf( berelement, new_fmt, (void * HOSTPTR)&str, (ber_len_t * HOSTPTR)&ber_len );
                if (!(*ptr = berstrdup_and_free( str ))) ret = LBER_ERROR;
                *len = ber_len;
                break;
            }
        case 'O':
            {
                struct berval *bv = NULL;
                BERVAL **ptr = va_arg( list, BERVAL ** );
                ret = ber_scanf( berelement, new_fmt, (void * HOSTPTR)&bv );
                if (bvconvert_and_free( 0, bv, ptr )) ret = LBER_ERROR;
                break;
            }
        case 'V':
            {
                struct berval ** HOSTPTR ptr = NULL;
                BERVAL ***array = va_arg( list, BERVAL *** );
                ret = ber_scanf( berelement, new_fmt, (void * HOSTPTR)&ptr );
                if (!(*array = bvecconvert_and_free( ptr ))) ret = LBER_ERROR;
                break;
            }
        case 'n':
        case 'x':
        case '{':
        case '}':
        case '[':
        case ']':
            ret = ber_scanf( berelement, new_fmt );
            break;
        default:
            FIXME( "Unknown format '%c'\n", new_fmt[0] );
            ret = -1;
            break;
        }
        if (ret == -1) break;
    }
    __ms_va_end( list );
    return ret;
#else
    return LBER_ERROR;
#endif
}
