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

#include <assert.h>
#include "wine/heap.h"
#include "wine/unicode.h"

extern HINSTANCE hwldap32 DECLSPEC_HIDDEN;

ULONG map_error( int ) DECLSPEC_HIDDEN;

#if defined(HAVE_LDAP) && !defined(__i386_on_x86_64__)
extern BerElement * (*pber_alloc_t)( int ) DECLSPEC_HIDDEN;
extern void (*pber_bvfree)( struct berval * ) DECLSPEC_HIDDEN;
extern ber_tag_t (*pber_first_element)( BerElement *, ber_len_t *, char ** ) DECLSPEC_HIDDEN;
extern int (*pber_flatten)( BerElement *, struct berval ** ) DECLSPEC_HIDDEN;
extern void (*pber_free)( BerElement *, int ) DECLSPEC_HIDDEN;
extern BerElement * (*pber_init)( struct berval * ) DECLSPEC_HIDDEN;
extern ber_tag_t (*pber_next_element)( BerElement *, ber_len_t *, LDAP_CONST char * ) DECLSPEC_HIDDEN;
extern ber_tag_t (*pber_peek_tag)( BerElement *, ber_len_t * ) DECLSPEC_HIDDEN;
extern ber_tag_t (*pber_skip_tag)( BerElement *, ber_len_t * ) DECLSPEC_HIDDEN;
extern int (*pber_printf)( BerElement *, LDAP_CONST char *, ... ) DECLSPEC_HIDDEN;
extern ber_tag_t (*pber_scanf)( BerElement *, LDAP_CONST char *, ... ) DECLSPEC_HIDDEN;

extern int (*pldap_abandon_ext)( LDAP *, int , LDAPControl **, LDAPControl ** ) DECLSPEC_HIDDEN;
extern int (*pldap_add_ext)( LDAP *, LDAP_CONST char *, LDAPMod **, LDAPControl **, LDAPControl **,
                             int * ) DECLSPEC_HIDDEN;
extern int (*pldap_add_ext_s)( LDAP *, LDAP_CONST char *, LDAPMod **, LDAPControl **, LDAPControl ** ) DECLSPEC_HIDDEN;
extern int (*pldap_compare_ext)( LDAP *, LDAP_CONST char *, LDAP_CONST char *, struct berval *, LDAPControl **,
                                 LDAPControl **, int * ) DECLSPEC_HIDDEN;
extern int (*pldap_compare_ext_s)( LDAP *, LDAP_CONST char *, LDAP_CONST char *, struct berval *, LDAPControl **,
                                   LDAPControl ** ) DECLSPEC_HIDDEN;
extern void (*pldap_control_free)( LDAPControl * ) DECLSPEC_HIDDEN;
extern void (*pldap_controls_free)( LDAPControl ** ) DECLSPEC_HIDDEN;
extern int (*pldap_count_entries)( LDAP *, LDAPMessage * ) DECLSPEC_HIDDEN;
extern int (*pldap_count_references)( LDAP *, LDAPMessage * ) DECLSPEC_HIDDEN;
extern int (*pldap_count_values_len)( struct berval ** ) DECLSPEC_HIDDEN;
extern int (*pldap_create_sort_control)( LDAP *, LDAPSortKey **, int, LDAPControl ** ) DECLSPEC_HIDDEN;
extern int (*pldap_create_vlv_control)( LDAP *, LDAPVLVInfo *, LDAPControl ** ) DECLSPEC_HIDDEN;
extern int (*pldap_delete_ext)( LDAP *, LDAP_CONST char *, LDAPControl **, LDAPControl **, int * ) DECLSPEC_HIDDEN;
extern int (*pldap_delete_ext_s)( LDAP *, LDAP_CONST char *, LDAPControl **, LDAPControl ** ) DECLSPEC_HIDDEN;
extern char * (*pldap_dn2ufn)( LDAP_CONST char * ) DECLSPEC_HIDDEN;
extern char ** (*pldap_explode_dn)( LDAP_CONST char *, int ) DECLSPEC_HIDDEN;
extern int (*pldap_extended_operation)( LDAP *, LDAP_CONST char *, struct berval *, LDAPControl **, LDAPControl **,
                                        int * ) DECLSPEC_HIDDEN;
extern int (*pldap_extended_operation_s)( LDAP *, LDAP_CONST char *, struct berval *, LDAPControl **, LDAPControl **,
                                          char **, struct berval ** ) DECLSPEC_HIDDEN;
extern char * (*pldap_first_attribute)( LDAP *, LDAPMessage *, BerElement ** ) DECLSPEC_HIDDEN;
extern LDAPMessage * (*pldap_first_entry)( LDAP *, LDAPMessage * ) DECLSPEC_HIDDEN;
extern LDAPMessage * (*pldap_first_reference)( LDAP *, LDAPMessage * ) DECLSPEC_HIDDEN;
extern char * (*pldap_get_dn)( LDAP *, LDAPMessage * ) DECLSPEC_HIDDEN;
extern int (*pldap_get_option)( LDAP *, int, void * ) DECLSPEC_HIDDEN;
extern struct berval ** (*pldap_get_values_len)( LDAP *, LDAPMessage *, LDAP_CONST char * ) DECLSPEC_HIDDEN;
extern int (*pldap_initialize)( LDAP **, LDAP_CONST char * ) DECLSPEC_HIDDEN;
extern void (*pldap_memfree)( void * ) DECLSPEC_HIDDEN;
extern void (*pldap_memvfree)( void ** ) DECLSPEC_HIDDEN;
extern int (*pldap_modify_ext)( LDAP *, LDAP_CONST char *, LDAPMod **, LDAPControl **, LDAPControl **,
                                int * ) DECLSPEC_HIDDEN;
extern int (*pldap_modify_ext_s)( LDAP *, LDAP_CONST char *, LDAPMod **, LDAPControl **,
                                  LDAPControl ** ) DECLSPEC_HIDDEN;
extern int (*pldap_msgfree)( LDAPMessage * ) DECLSPEC_HIDDEN;
extern char * (*pldap_next_attribute)( LDAP *, LDAPMessage *, BerElement * ) DECLSPEC_HIDDEN;
extern LDAPMessage * (*pldap_next_entry)( LDAP *, LDAPMessage * ) DECLSPEC_HIDDEN;
extern LDAPMessage * (*pldap_next_reference)( LDAP *, LDAPMessage * ) DECLSPEC_HIDDEN;
extern int (*pldap_parse_extended_result)( LDAP *, LDAPMessage *, char **, struct berval **, int ) DECLSPEC_HIDDEN;
extern int (*pldap_parse_reference)( LDAP *, LDAPMessage *, char ***, LDAPControl ***, int ) DECLSPEC_HIDDEN;
extern int (*pldap_parse_result)( LDAP *, LDAPMessage *, int *, char **, char **, char ***, LDAPControl ***,
                                  int ) DECLSPEC_HIDDEN;
extern int (*pldap_parse_sort_control)( LDAP *, LDAPControl **, unsigned long *, char ** ) DECLSPEC_HIDDEN;
extern int (*pldap_parse_sortresponse_control)( LDAP *, LDAPControl *, ber_int_t *, char ** ) DECLSPEC_HIDDEN;
extern int (*pldap_parse_vlv_control)( LDAP *, LDAPControl **, unsigned long *, unsigned long *, struct berval **, int * ) DECLSPEC_HIDDEN;
extern int (*pldap_parse_vlvresponse_control)( LDAP *, LDAPControl *, ber_int_t *, ber_int_t *, struct berval **,
                                               int * ) DECLSPEC_HIDDEN;
extern int (*pldap_rename)( LDAP *, LDAP_CONST char *, LDAP_CONST char *, LDAP_CONST char *, int, LDAPControl **,
                            LDAPControl **, int * ) DECLSPEC_HIDDEN;
extern int (*pldap_rename_s)( LDAP *, LDAP_CONST char *, LDAP_CONST char *, LDAP_CONST char *, int, LDAPControl **,
                              LDAPControl ** ) DECLSPEC_HIDDEN;
extern int (*pldap_result)( LDAP *, int, int, struct timeval *, LDAPMessage ** ) DECLSPEC_HIDDEN;
extern int (*pldap_sasl_bind)( LDAP *, LDAP_CONST char *, LDAP_CONST char *, struct berval *, LDAPControl **,
                               LDAPControl **, int * ) DECLSPEC_HIDDEN;
extern int (*pldap_sasl_bind_s)( LDAP *, LDAP_CONST char *, LDAP_CONST char *, struct berval *, LDAPControl **,
                                 LDAPControl **, struct berval ** ) DECLSPEC_HIDDEN;
typedef int (LDAP_SASL_INTERACT_PROC)( LDAP *, unsigned, void *, void * );
extern int (*pldap_sasl_interactive_bind_s)( LDAP *, LDAP_CONST char *, LDAP_CONST char *, LDAPControl **, LDAPControl **,
                                             unsigned, LDAP_SASL_INTERACT_PROC *, void * ) DECLSPEC_HIDDEN;
extern int (*pldap_search_ext)( LDAP *, LDAP_CONST char *, int, LDAP_CONST char *, char **, int, LDAPControl **,
                                LDAPControl **, struct timeval *, int, int * ) DECLSPEC_HIDDEN;
extern int (*pldap_search_ext_s)( LDAP *, LDAP_CONST char *, int, LDAP_CONST char *, char **, int, LDAPControl **,
                                  LDAPControl **, struct timeval *, int, LDAPMessage ** ) DECLSPEC_HIDDEN;
extern int (*pldap_set_option)( LDAP *, int, LDAP_CONST void * ) DECLSPEC_HIDDEN;
extern int (*pldap_start_tls_s)( LDAP *, LDAPControl **, LDAPControl ** ) DECLSPEC_HIDDEN;
extern int (*pldap_unbind_ext)( LDAP *, LDAPControl **, LDAPControl ** ) DECLSPEC_HIDDEN;
extern int (*pldap_unbind_ext_s)( LDAP *, LDAPControl **, LDAPControl ** ) DECLSPEC_HIDDEN;
extern void (*pldap_value_free_len)( struct berval ** ) DECLSPEC_HIDDEN;
#endif

/* A set of helper functions to convert LDAP data structures
 * to and from ansi (A), wide character (W) and utf8 (U) encodings.
 */

static inline char *strdupU( const char *src )
{
    char *dst;
    if (!src) return NULL;
    if ((dst = heap_alloc( (strlen( src ) + 1) * sizeof(char) ))) strcpy( dst, src );
    return dst;
}

static inline WCHAR *strdupW( const WCHAR *src )
{
    WCHAR *dst;
    if (!src) return NULL;
    if ((dst = heap_alloc( (strlenW( src ) + 1) * sizeof(WCHAR) ))) strcpyW( dst, src );
    return dst;
}

static inline LPWSTR strAtoW( LPCSTR str )
{
    LPWSTR ret = NULL;
    if (str)
    {
        DWORD len = MultiByteToWideChar( CP_ACP, 0, str, -1, NULL, 0 );
        if ((ret = heap_alloc( len * sizeof(WCHAR) )))
            MultiByteToWideChar( CP_ACP, 0, str, -1, ret, len );
    }
    return ret;
}

static inline LPSTR strWtoA( LPCWSTR str )
{
    LPSTR ret = NULL;
    if (str)
    {
        DWORD len = WideCharToMultiByte( CP_ACP, 0, str, -1, NULL, 0, NULL, NULL );
        if ((ret = heap_alloc( len )))
            WideCharToMultiByte( CP_ACP, 0, str, -1, ret, len, NULL, NULL );
    }
    return ret;
}

static inline char *strWtoU( LPCWSTR str )
{
    LPSTR ret = NULL;
    if (str)
    {
        DWORD len = WideCharToMultiByte( CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL );
        if ((ret = heap_alloc( len )))
            WideCharToMultiByte( CP_UTF8, 0, str, -1, ret, len, NULL, NULL );
    }
    return ret;
}

static inline LPWSTR strUtoW( char *str )
{
    LPWSTR ret = NULL;
    if (str)
    {
        DWORD len = MultiByteToWideChar( CP_UTF8, 0, str, -1, NULL, 0 );
        if ((ret = heap_alloc( len * sizeof(WCHAR) )))
            MultiByteToWideChar( CP_UTF8, 0, str, -1, ret, len );
    }
    return ret;
}

static inline LPWSTR strnAtoW( LPCSTR str, DWORD inlen, DWORD *outlen )
{
    LPWSTR ret = NULL;
    *outlen = 0;
    if (str)
    {
        DWORD len = MultiByteToWideChar( CP_ACP, 0, str, inlen, NULL, 0 );
        if ((ret = heap_alloc( len * sizeof(WCHAR) )))
        {
            MultiByteToWideChar( CP_ACP, 0, str, inlen, ret, len );
            *outlen = len;
        }
    }
    return ret;
}

static inline char *strnWtoU( LPCWSTR str, DWORD inlen, DWORD *outlen )
{
    LPSTR ret = NULL;
    *outlen = 0;
    if (str)
    {
        DWORD len = WideCharToMultiByte( CP_UTF8, 0, str, inlen, NULL, 0, NULL, NULL );
        if ((ret = heap_alloc( len )))
        {
            WideCharToMultiByte( CP_UTF8, 0, str, inlen, ret, len, NULL, NULL );
            *outlen = len;
        }
    }
    return ret;
}

static inline void strfreeA( LPSTR str )
{
    heap_free( str );
}

static inline void strfreeW( LPWSTR str )
{
    heap_free( str );
}

static inline void strfreeU( char *str )
{
    heap_free( str );
}

static inline DWORD strarraylenA( LPSTR *strarray )
{
    LPSTR *p = strarray;
    while (*p) p++;
    return p - strarray;
}

static inline DWORD strarraylenW( LPWSTR *strarray )
{
    LPWSTR *p = strarray;
    while (*p) p++;
    return p - strarray;
}

static inline DWORD strarraylenU( char **strarray )
{
    char **p = strarray;
    while (*p) p++;
    return p - strarray;
}

static inline LPWSTR *strarrayAtoW( LPSTR *strarray )
{
    LPWSTR *strarrayW = NULL;
    DWORD size;

    if (strarray)
    {
        size  = sizeof(WCHAR*) * (strarraylenA( strarray ) + 1);
        if ((strarrayW = heap_alloc( size )))
        {
            LPSTR *p = strarray;
            LPWSTR *q = strarrayW;

            while (*p) *q++ = strAtoW( *p++ );
            *q = NULL;
        }
    }
    return strarrayW;
}

static inline LPSTR *strarrayWtoA( LPWSTR *strarray )
{
    LPSTR *strarrayA = NULL;
    DWORD size;

    if (strarray)
    {
        size = sizeof(LPSTR) * (strarraylenW( strarray ) + 1);
        if ((strarrayA = heap_alloc( size )))
        {
            LPWSTR *p = strarray;
            LPSTR *q = strarrayA;

            while (*p) *q++ = strWtoA( *p++ );
            *q = NULL;
        }
    }
    return strarrayA;
}

/* 32on64 FIXME: Is this right? */
static inline char * HOSTPTR *strarrayWtoU( LPWSTR *strarray )
{
    char * HOSTPTR *strarrayU = NULL;
    DWORD size;

    if (strarray)
    {
        size = sizeof(char*) * (strarraylenW( strarray ) + 1);
        if ((strarrayU = heap_alloc( size )))
        {
            LPWSTR *p = strarray;
            char * HOSTPTR *q = strarrayU;

            while (*p) *q++ = strWtoU( *p++ );
            *q = NULL;
        }
    }
    return strarrayU;
}

static inline LPWSTR *strarrayUtoW( char **strarray )
{
    LPWSTR *strarrayW = NULL;
    DWORD size;

    if (strarray)
    {
        size = sizeof(WCHAR*) * (strarraylenU( strarray ) + 1);
        if ((strarrayW = heap_alloc( size )))
        {
            char **p = strarray;
            LPWSTR *q = strarrayW;

            while (*p) *q++ = strUtoW( *p++ );
            *q = NULL;
        }
    }
    return strarrayW;
}

static inline LPWSTR *strarraydupW( LPWSTR *strarray )
{
    LPWSTR *strarrayW = NULL;
    DWORD size;

    if (strarray)
    {
        size = sizeof(WCHAR*) * (strarraylenW( strarray ) + 1);
        if ((strarrayW = heap_alloc( size )))
        {
            LPWSTR *p = strarray;
            LPWSTR *q = strarrayW;

            while (*p) *q++ = strdupW( *p++ );
            *q = NULL;
        }
    }
    return strarrayW;
}

static inline void strarrayfreeA( LPSTR *strarray )
{
    if (strarray)
    {
        LPSTR *p = strarray;
        while (*p) strfreeA( *p++ );
        heap_free( strarray );
    }
}

static inline void strarrayfreeW( LPWSTR *strarray )
{
    if (strarray)
    {
        LPWSTR *p = strarray;
        while (*p) strfreeW( *p++ );
        heap_free( strarray );
    }
}

static inline void strarrayfreeU( char **strarray )
{
    if (strarray)
    {
        char **p = strarray;
        while (*p) strfreeU( *p++ );
        heap_free( strarray );
    }
}

static inline struct WLDAP32_berval *bervalWtoW( struct WLDAP32_berval *bv )
{
    struct WLDAP32_berval *berval;
    DWORD size = sizeof(*berval) + bv->bv_len;

    if ((berval = heap_alloc( size )))
    {
        char *val = (char *)(berval + 1);

        berval->bv_len = bv->bv_len;
        berval->bv_val = val;
        memcpy( val, bv->bv_val, bv->bv_len );
    }
    return berval;
}

static inline void bvarrayfreeW( struct WLDAP32_berval **bv )
{
    struct WLDAP32_berval **p = bv;
    while (*p) heap_free( *p++ );
    heap_free( bv );
}

#if defined(HAVE_LDAP) && !defined(__i386_on_x86_64__)

static inline struct berval *bervalWtoU( struct WLDAP32_berval *bv )
{
    struct berval *berval;
    DWORD size = sizeof(*berval) + bv->bv_len;

    if ((berval = heap_alloc( size )))
    {
        char * HOSTPTR val = (char * HOSTPTR)berval + sizeof(struct berval);

        berval->bv_len = bv->bv_len;
        berval->bv_val = val;
        memcpy( val, bv->bv_val, bv->bv_len );
    }
    return berval;
}

static inline struct WLDAP32_berval *bervalUtoW( struct berval *bv )
{
    struct WLDAP32_berval *berval;
    DWORD size = sizeof(*berval) + bv->bv_len;

    assert( bv->bv_len <= ~0u );

    if ((berval = heap_alloc( size )))
    {
        char *val = (char *)(berval + 1);

        berval->bv_len = bv->bv_len;
        berval->bv_val = val;
        memcpy( val, bv->bv_val, bv->bv_len );
    }
    return berval;
}

static inline DWORD bvarraylenU( struct berval **bv )
{
    struct berval **p = bv;
    while (*p) p++;
    return p - bv;
}

static inline DWORD bvarraylenW( struct WLDAP32_berval **bv )
{
    struct WLDAP32_berval **p = bv;
    while (*p) p++;
    return p - bv;
}

static inline struct WLDAP32_berval **bvarrayWtoW( struct WLDAP32_berval **bv )
{
    struct WLDAP32_berval **berval = NULL;
    DWORD size;

    if (bv)
    {
        size = sizeof(*berval) * (bvarraylenW( bv ) + 1);
        if ((berval = heap_alloc( size )))
        {
            struct WLDAP32_berval **p = bv;
            struct WLDAP32_berval **q = berval;

            while (*p) *q++ = bervalWtoW( *p++ );
            *q = NULL;
        }
    }
    return berval;
}

static inline struct berval **bvarrayWtoU( struct WLDAP32_berval **bv )
{
    struct berval **berval = NULL;
    DWORD size;

    if (bv)
    {
        size = sizeof(*berval) * (bvarraylenW( bv ) + 1);
        if ((berval = heap_alloc( size )))
        {
            struct WLDAP32_berval **p = bv;
            struct berval **q = berval;

            while (*p) *q++ = bervalWtoU( *p++ );
            *q = NULL;
        }
    }
    return berval;
}

static inline struct WLDAP32_berval **bvarrayUtoW( struct berval **bv )
{
    struct WLDAP32_berval **berval = NULL;
    DWORD size;

    if (bv)
    {
        size = sizeof(*berval) * (bvarraylenU( bv ) + 1);
        if ((berval = heap_alloc( size )))
        {
            struct berval **p = bv;
            struct WLDAP32_berval **q = berval;

            while (*p) *q++ = bervalUtoW( *p++ );
            *q = NULL;
        }
    }
    return berval;
}

static inline void bvarrayfreeU( struct berval **bv )
{
    struct berval **p = bv;
    while (*p) heap_free( *p++ );
    heap_free( bv );
}

static inline LDAPModW *modAtoW( LDAPModA *mod )
{
    LDAPModW *modW;

    if ((modW = heap_alloc( sizeof(LDAPModW) )))
    {
        modW->mod_op = mod->mod_op;
        modW->mod_type = strAtoW( mod->mod_type );

        if (mod->mod_op & LDAP_MOD_BVALUES)
            modW->mod_vals.modv_bvals = bvarrayWtoW( mod->mod_vals.modv_bvals );
        else
            modW->mod_vals.modv_strvals = strarrayAtoW( mod->mod_vals.modv_strvals );
    }
    return modW;
}

static inline LDAPMod *modWtoU( LDAPModW *mod )
{
    LDAPMod *modU;

    if ((modU = heap_alloc( sizeof(LDAPMod) )))
    {
        modU->mod_op = mod->mod_op;
        modU->mod_type = strWtoU( mod->mod_type );

        if (mod->mod_op & LDAP_MOD_BVALUES)
            modU->mod_vals.modv_bvals = bvarrayWtoU( mod->mod_vals.modv_bvals );
        else
            modU->mod_vals.modv_strvals = strarrayWtoU( mod->mod_vals.modv_strvals );
    }
    return modU;
}

static inline void modfreeW( LDAPModW *mod )
{
    if (mod->mod_op & LDAP_MOD_BVALUES)
        bvarrayfreeW( mod->mod_vals.modv_bvals );
    else
        strarrayfreeW( mod->mod_vals.modv_strvals );
    heap_free( mod );
}

static inline struct WLDAP32_berval *bvconvert_h2w( struct berval *bv )
{
    struct WLDAP32_berval *bvconv = NULL;

    if (bv)
    {
        DWORD size = sizeof(struct WLDAP32_berval) + bv->bv_len;
        if ((bvconv = heap_alloc( size )))
        {
            char *val = (char *)bvconv + sizeof(struct WLDAP32_berval);

            bvconv->bv_len = bv->bv_len;
            bvconv->bv_val = val;
            memcpy( val, bv->bv_val, bv->bv_len );
        }
    }
    return bvconv;
}

static inline ULONG bvconvert_and_free( ULONG ret, struct berval *bv, struct WLDAP32_berval **ptr )
{
    *ptr = bvconvert_h2w( bv );
    pber_bvfree( bv );
    return (bv && !*ptr) ? WLDAP32_LDAP_NO_MEMORY : ret;
}

static inline void modfreeU( LDAPMod *mod )
{
    if (mod->mod_op & LDAP_MOD_BVALUES)
        bvarrayfreeU( ADDRSPACECAST(void *, mod->mod_vals.modv_bvals) );
    else
        strarrayfreeU( ADDRSPACECAST(void *, mod->mod_vals.modv_strvals) );
    heap_free( mod );
}

static inline DWORD modarraylenA( LDAPModA **modarray )
{
    LDAPModA **p = modarray;
    while (*p) p++;
    return p - modarray;
}

static inline DWORD modarraylenW( LDAPModW **modarray )
{
    LDAPModW **p = modarray;
    while (*p) p++;
    return p - modarray;
}

static inline LDAPModW **modarrayAtoW( LDAPModA **modarray )
{
    LDAPModW **modarrayW = NULL;
    DWORD size;

    if (modarray)
    {
        size = sizeof(LDAPModW*) * (modarraylenA( modarray ) + 1);
        if ((modarrayW = heap_alloc( size )))
        {
            LDAPModA **p = modarray;
            LDAPModW **q = modarrayW;

            while (*p) *q++ = modAtoW( *p++ );
            *q = NULL;
        }
    }
    return modarrayW;
}

static inline LDAPMod **modarrayWtoU( LDAPModW **modarray )
{
    LDAPMod **modarrayU = NULL;
    DWORD size;

    if (modarray)
    {
        size = sizeof(LDAPMod*) * (modarraylenW( modarray ) + 1);
        if ((modarrayU = heap_alloc( size )))
        {
            LDAPModW **p = modarray;
            LDAPMod **q = modarrayU;

            while (*p) *q++ = modWtoU( *p++ );
            *q = NULL;
        }
    }
    return modarrayU;
}

static inline void modarrayfreeW( LDAPModW **modarray )
{
    if (modarray)
    {
        LDAPModW **p = modarray;
        while (*p) modfreeW( *p++ );
        heap_free( modarray );
    }
}

static inline void modarrayfreeU( LDAPMod **modarray )
{
    if (modarray)
    {
        LDAPMod **p = modarray;
        while (*p) modfreeU( *p++ );
        heap_free( modarray );
    }
}

static inline LDAPControlW *controlAtoW( LDAPControlA *control )
{
    LDAPControlW *controlW;
    DWORD len = control->ldctl_value.bv_len;
    char *val = NULL;

    if (control->ldctl_value.bv_val)
    {
        if (!(val = heap_alloc( len ))) return NULL;
        memcpy( val, control->ldctl_value.bv_val, len );
    }

    if (!(controlW = heap_alloc( sizeof(LDAPControlW) )))
    {
        heap_free( val );
        return NULL;
    }

    controlW->ldctl_oid = strAtoW( control->ldctl_oid );
    controlW->ldctl_value.bv_len = len; 
    controlW->ldctl_value.bv_val = val; 
    controlW->ldctl_iscritical = control->ldctl_iscritical;

    return controlW;
}

static inline LDAPControlA *controlWtoA( LDAPControlW *control )
{
    LDAPControlA *controlA;
    DWORD len = control->ldctl_value.bv_len;
    char *val = NULL;

    if (control->ldctl_value.bv_val)
    {
        if (!(val = heap_alloc( len ))) return NULL;
        memcpy( val, control->ldctl_value.bv_val, len );
    }

    if (!(controlA = heap_alloc( sizeof(LDAPControlA) )))
    {
        heap_free( val );
        return NULL;
    }

    controlA->ldctl_oid = strWtoA( control->ldctl_oid );
    controlA->ldctl_value.bv_len = len; 
    controlA->ldctl_value.bv_val = val;
    controlA->ldctl_iscritical = control->ldctl_iscritical;

    return controlA;
}

static inline LDAPControl *controlWtoU( LDAPControlW *control )
{
    LDAPControl *controlU;
    DWORD len = control->ldctl_value.bv_len;
    char *val = NULL;

    if (control->ldctl_value.bv_val)
    {
        if (!(val = heap_alloc( len ))) return NULL;
        memcpy( val, control->ldctl_value.bv_val, len );
    }

    if (!(controlU = heap_alloc( sizeof(LDAPControl) )))
    {
        heap_free( val );
        return NULL;
    }

    controlU->ldctl_oid = strWtoU( control->ldctl_oid );
    controlU->ldctl_value.bv_len = len; 
    controlU->ldctl_value.bv_val = val; 
    controlU->ldctl_iscritical = control->ldctl_iscritical;

    return controlU;
}

static inline LDAPControlW *controlUtoW( LDAPControl *control )
{
    LDAPControlW *controlW;
    DWORD len = control->ldctl_value.bv_len;
    char *val = NULL, *copy;

    if (control->ldctl_value.bv_val)
    {
        if (!(val = heap_alloc( len ))) return NULL;
        memcpy( val, control->ldctl_value.bv_val, len );
    }

    if (!(controlW = heap_alloc( sizeof(LDAPControlW) )))
    {
        heap_free( val );
        return NULL;
    }

    copy = heap_strdup(control->ldctl_oid);
    controlW->ldctl_oid = strUtoW( copy );
    heap_free(copy);
    controlW->ldctl_value.bv_len = len;
    controlW->ldctl_value.bv_val = val;
    controlW->ldctl_iscritical = control->ldctl_iscritical;

    return controlW;
}

static inline LDAPControlW *controldupW( LDAPControlW *control )
{
    LDAPControlW *controlW;
    DWORD len = control->ldctl_value.bv_len;
    char *val = NULL;

    if (control->ldctl_value.bv_val)
    {
        if (!(val = heap_alloc( len ))) return NULL;
        memcpy( val, control->ldctl_value.bv_val, len );
    }

    if (!(controlW = heap_alloc( sizeof(LDAPControlW) )))
    {
        heap_free( val );
        return NULL;
    }

    controlW->ldctl_oid = strdupW( control->ldctl_oid );
    controlW->ldctl_value.bv_len = len;
    controlW->ldctl_value.bv_val = val;
    controlW->ldctl_iscritical = control->ldctl_iscritical;

    return controlW;
}

static inline void controlfreeA( LDAPControlA *control )
{
    if (control)
    {
        strfreeA( control->ldctl_oid );
        heap_free( control->ldctl_value.bv_val );
        heap_free( control );
    }
}

static inline void controlfreeW( LDAPControlW *control )
{
    if (control)
    {
        strfreeW( control->ldctl_oid );
        heap_free( control->ldctl_value.bv_val );
        heap_free( control );
    }
}

static inline void controlfreeU( LDAPControl *control )
{
    if (control)
    {
        strfreeU( ADDRSPACECAST(void *, control->ldctl_oid) );
        heap_free( control->ldctl_value.bv_val );
        heap_free( control );
    }
}

static inline DWORD controlarraylenA( LDAPControlA **controlarray )
{
    LDAPControlA **p = controlarray;
    while (*p) p++;
    return p - controlarray;
}

static inline DWORD controlarraylenW( LDAPControlW **controlarray )
{
    LDAPControlW **p = controlarray;
    while (*p) p++;
    return p - controlarray;
}

static inline DWORD controlarraylenU( LDAPControl **controlarray )
{
    LDAPControl **p = controlarray;
    while (*p) p++;
    return p - controlarray;
}

static inline LDAPControlW **controlarrayAtoW( LDAPControlA **controlarray )
{
    LDAPControlW **controlarrayW = NULL;
    DWORD size;

    if (controlarray)
    {
        size = sizeof(LDAPControlW*) * (controlarraylenA( controlarray ) + 1);
        if ((controlarrayW = heap_alloc( size )))
        {
            LDAPControlA **p = controlarray;
            LDAPControlW **q = controlarrayW;

            while (*p) *q++ = controlAtoW( *p++ );
            *q = NULL;
        }
    }
    return controlarrayW;
}

static inline LDAPControlA **controlarrayWtoA( LDAPControlW **controlarray )
{
    LDAPControlA **controlarrayA = NULL;
    DWORD size;

    if (controlarray)
    {
        size = sizeof(LDAPControl*) * (controlarraylenW( controlarray ) + 1);
        if ((controlarrayA = heap_alloc( size )))
        {
            LDAPControlW **p = controlarray;
            LDAPControlA **q = controlarrayA;

            while (*p) *q++ = controlWtoA( *p++ );
            *q = NULL;
        }
    }
    return controlarrayA;
}

static inline LDAPControl **controlarrayWtoU( LDAPControlW **controlarray )
{
    LDAPControl **controlarrayU = NULL;
    DWORD size;

    if (controlarray)
    {
        size = sizeof(LDAPControl*) * (controlarraylenW( controlarray ) + 1);
        if ((controlarrayU = heap_alloc( size )))
        {
            LDAPControlW **p = controlarray;
            LDAPControl **q = controlarrayU;

            while (*p) *q++ = controlWtoU( *p++ );
            *q = NULL;
        }
    }
    return controlarrayU;
}

static inline LDAPControlW **controlarrayUtoW( LDAPControl **controlarray )
{
    LDAPControlW **controlarrayW = NULL;
    DWORD size;

    if (controlarray)
    {
        size = sizeof(LDAPControlW*) * (controlarraylenU( controlarray ) + 1);
        if ((controlarrayW = heap_alloc( size )))
        {
            LDAPControl **p = controlarray;
            LDAPControlW **q = controlarrayW;

            while (*p) *q++ = controlUtoW( *p++ );
            *q = NULL;
        }
    }
    return controlarrayW;
}

static inline LDAPControlW **controlarraydupW( LDAPControlW **controlarray )
{
    LDAPControlW **controlarrayW = NULL;
    DWORD size;

    if (controlarray)
    {
        size = sizeof(LDAPControlW*) * (controlarraylenW( controlarray ) + 1);
        if ((controlarrayW = heap_alloc( size )))
        {
            LDAPControlW **p = controlarray;
            LDAPControlW **q = controlarrayW;

            while (*p) *q++ = controldupW( *p++ );
            *q = NULL;
        }
    }
    return controlarrayW;
}

static inline void controlarrayfreeA( LDAPControlA **controlarray )
{
    if (controlarray)
    {
        LDAPControlA **p = controlarray;
        while (*p) controlfreeA( *p++ );
        heap_free( controlarray );
    }
}

static inline void controlarrayfreeW( LDAPControlW **controlarray )
{
    if (controlarray)
    {
        LDAPControlW **p = controlarray;
        while (*p) controlfreeW( *p++ );
        heap_free( controlarray );
    }
}

static inline void controlarrayfreeU( LDAPControl **controlarray )
{
    if (controlarray)
    {
        LDAPControl **p = controlarray;
        while (*p) controlfreeU( *p++ );
        heap_free( controlarray );
    }
}

static inline LDAPSortKeyW *sortkeyAtoW( LDAPSortKeyA *sortkey )
{
    LDAPSortKeyW *sortkeyW;

    if ((sortkeyW = heap_alloc( sizeof(LDAPSortKeyW) )))
    {
        sortkeyW->sk_attrtype = strAtoW( sortkey->sk_attrtype );
        sortkeyW->sk_matchruleoid = strAtoW( sortkey->sk_matchruleoid );
        sortkeyW->sk_reverseorder = sortkey->sk_reverseorder;
    }
    return sortkeyW;
}

static inline LDAPSortKeyA *sortkeyWtoA( LDAPSortKeyW *sortkey )
{
    LDAPSortKeyA *sortkeyA;

    if ((sortkeyA = heap_alloc( sizeof(LDAPSortKeyA) )))
    {
        sortkeyA->sk_attrtype = strWtoA( sortkey->sk_attrtype );
        sortkeyA->sk_matchruleoid = strWtoA( sortkey->sk_matchruleoid );
        sortkeyA->sk_reverseorder = sortkey->sk_reverseorder;
    }
    return sortkeyA;
}

static inline LDAPSortKey *sortkeyWtoU( LDAPSortKeyW *sortkey )
{
    LDAPSortKey *sortkeyU;

    if ((sortkeyU = heap_alloc( sizeof(LDAPSortKey) )))
    {
        sortkeyU->attributeType = strWtoU( sortkey->sk_attrtype );
        sortkeyU->orderingRule = strWtoU( sortkey->sk_matchruleoid );
        sortkeyU->reverseOrder = sortkey->sk_reverseorder;
    }
    return sortkeyU;
}

static inline void sortkeyfreeA( LDAPSortKeyA *sortkey )
{
    if (sortkey)
    {
        strfreeA( sortkey->sk_attrtype );
        strfreeA( sortkey->sk_matchruleoid );
        heap_free( sortkey );
    }
}

static inline void sortkeyfreeW( LDAPSortKeyW *sortkey )
{
    if (sortkey)
    {
        strfreeW( sortkey->sk_attrtype );
        strfreeW( sortkey->sk_matchruleoid );
        heap_free( sortkey );
    }
}

static inline void sortkeyfreeU( LDAPSortKey *sortkey )
{
    if (sortkey)
    {
        strfreeU( ADDRSPACECAST(void *, sortkey->attributeType) );
        strfreeU( ADDRSPACECAST(void *, sortkey->orderingRule) );
        heap_free( sortkey );
    }
}

static inline DWORD sortkeyarraylenA( LDAPSortKeyA **sortkeyarray )
{
    LDAPSortKeyA **p = sortkeyarray;
    while (*p) p++;
    return p - sortkeyarray;
}

static inline DWORD sortkeyarraylenW( LDAPSortKeyW **sortkeyarray )
{
    LDAPSortKeyW **p = sortkeyarray;
    while (*p) p++;
    return p - sortkeyarray;
}

static inline LDAPSortKeyW **sortkeyarrayAtoW( LDAPSortKeyA **sortkeyarray )
{
    LDAPSortKeyW **sortkeyarrayW = NULL;
    DWORD size;

    if (sortkeyarray)
    {
        size = sizeof(LDAPSortKeyW*) * (sortkeyarraylenA( sortkeyarray ) + 1);
        if ((sortkeyarrayW = heap_alloc( size )))
        {
            LDAPSortKeyA **p = sortkeyarray;
            LDAPSortKeyW **q = sortkeyarrayW;

            while (*p) *q++ = sortkeyAtoW( *p++ );
            *q = NULL;
        }
    }
    return sortkeyarrayW;
}

static inline LDAPSortKey **sortkeyarrayWtoU( LDAPSortKeyW **sortkeyarray )
{
    LDAPSortKey **sortkeyarrayU = NULL;
    DWORD size;

    if (sortkeyarray)
    {
        size = sizeof(LDAPSortKey*) * (sortkeyarraylenW( sortkeyarray ) + 1);
        if ((sortkeyarrayU = heap_alloc( size )))
        {
            LDAPSortKeyW **p = sortkeyarray;
            LDAPSortKey **q = sortkeyarrayU;

            while (*p) *q++ = sortkeyWtoU( *p++ );
            *q = NULL;
        }
    }
    return sortkeyarrayU;
}

static inline void sortkeyarrayfreeW( LDAPSortKeyW **sortkeyarray )
{
    if (sortkeyarray)
    {
        LDAPSortKeyW **p = sortkeyarray;
        while (*p) sortkeyfreeW( *p++ );
        heap_free( sortkeyarray );
    }
}

static inline void sortkeyarrayfreeU( LDAPSortKey **sortkeyarray )
{
    if (sortkeyarray)
    {
        LDAPSortKey **p = sortkeyarray;
        while (*p) sortkeyfreeU( *p++ );
        heap_free( sortkeyarray );
    }
}
#endif /* HAVE_LDAP */
