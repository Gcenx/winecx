/*
 * WLDAP32 - LDAP support for Wine
 *
 * Copyright 2005 Hans Leidekker
 * Copyright 2019 Conor McCarthy for Codeweavers
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

#include "wine/heap.h"
#include "wine/unicode.h"

extern HINSTANCE hwldap32 DECLSPEC_HIDDEN;

ULONG map_error( int ) DECLSPEC_HIDDEN;

/* A set of helper functions to convert LDAP data structures
 * to and from ansi (A), wide character (W) and utf8 (U) encodings.
 */

static inline char *strdupU( const char * HOSTPTR src )
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

static inline LPWSTR strUtoW( char * HOSTPTR str )
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

static inline void strfreeA( LPSTR str )
{
    heap_free( str );
}

static inline void strfreeW( LPWSTR str )
{
    heap_free( str );
}

static inline void strfreeU( char * HOSTPTR str )
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

static inline DWORD strarraylenU( char * HOSTPTR * HOSTPTR strarray )
{
    char * HOSTPTR * HOSTPTR p = strarray;
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

static inline char * HOSTPTR * strarrayWtoU( LPWSTR *strarray )
{
    char * HOSTPTR * strarrayU = NULL;
    DWORD size;

    if (strarray)
    {
        size = sizeof(char* HOSTPTR) * (strarraylenW( strarray ) + 1);
        if ((strarrayU = heap_alloc( size )))
        {
            LPWSTR *p = strarray;
            char * HOSTPTR * q = strarrayU;

            while (*p) *q++ = strWtoU( *p++ );
            *q = NULL;
        }
    }
    return strarrayU;
}

static inline LPWSTR *strarrayUtoW( char * HOSTPTR * HOSTPTR strarray )
{
    LPWSTR *strarrayW = NULL;
    DWORD size;

    if (strarray)
    {
        size = sizeof(WCHAR*) * (strarraylenU( strarray ) + 1);
        if ((strarrayW = heap_alloc( size )))
        {
            char * HOSTPTR * HOSTPTR p = strarray;
            LPWSTR *q = strarrayW;

            while (*p) *q++ = strUtoW( *p++ );
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

static inline void strarrayfreeU( char * HOSTPTR * HOSTPTR strarray )
{
    if (strarray)
    {
        char * HOSTPTR * HOSTPTR p = strarray;
        while (*p) strfreeU( *p++ );
        heap_free( strarray );
    }
}

#ifdef HAVE_LDAP

static inline struct WLDAP32_berval *bvdup( struct WLDAP32_berval *bv )
{
    struct WLDAP32_berval *berval;
    DWORD size = sizeof(struct WLDAP32_berval) + bv->bv_len;

    if ((berval = heap_alloc( size )))
    {
        char *val = (char *)berval + sizeof(struct WLDAP32_berval);

        berval->bv_len = bv->bv_len;
        berval->bv_val = val;
        memcpy( val, bv->bv_val, bv->bv_len );
    }
    return berval;
}

static inline DWORD bvarraylen_w( struct WLDAP32_berval **bv )
{
    struct WLDAP32_berval **p = bv;
    while (*p) p++;
    return p - bv;
}

static inline struct WLDAP32_berval **bvarraydup( struct WLDAP32_berval **bv )
{
    struct WLDAP32_berval **berval = NULL;
    DWORD size;

    if (bv)
    {
        size = sizeof(struct WLDAP32_berval *) * (bvarraylen_w( bv ) + 1);
        if ((berval = heap_alloc( size )))
        {
            struct WLDAP32_berval **p = bv;
            struct WLDAP32_berval **q = berval;

            while (*p) *q++ = bvdup( *p++ );
            *q = NULL;
        }
    }
    return berval;
}

static inline void bvarrayfree_w( struct WLDAP32_berval **bv )
{
    struct WLDAP32_berval **p = bv;
    while (*p) heap_free( *p++ );
    heap_free( bv );
}

#ifdef __i386_on_x86_64__

static inline DWORD strarraylenU( char **strarray ) __attribute__((overloadable))
{
    char **p = strarray;
    while (*p) p++;
    return p - strarray;
}

static inline char *berstrdup_and_free( char * HOSTPTR src )
{
    char *str = strdupU( src );
    ber_memfree( src );
    return str;
}

static inline char **berstrarraydup_and_free( char * HOSTPTR * HOSTPTR strarray )
{
    char **strarrayU = NULL;
    DWORD size;

    if (strarray)
    {
        size = sizeof(char *) * (strarraylenU( strarray ) + 1);
        if ((strarrayU = heap_alloc( size )))
        {
            char * HOSTPTR * HOSTPTR p = strarray;
            char **q = strarrayU;

            while (*p) *q++ = strdupU( *p++ );
            *q = NULL;
        }
        ber_memvfree( (void * HOSTPTR * HOSTPTR)strarray );
    }
    return strarrayU;
}

static inline char * HOSTPTR * strhostarray( char **strarray )
{
    if (strarray)
    {
        DWORD size = sizeof(char * HOSTPTR) * (strarraylenU( strarray ) + 1);
        char * HOSTPTR *array = heap_alloc( size );
        char * HOSTPTR *q = array;
        char **p = strarray;
        if (!array) return NULL;
        while (*p) *q++ = *p++;
        *q = NULL;
        return array;
    }
    return NULL;
}

static inline void strhostarrayfree( void * HOSTPTR ptr )
{
    heap_free( ptr );
}

static inline void strarrayfreeU( char **strarray ) __attribute__((overloadable))
{
    if (strarray)
    {
        char **p = strarray;
        while (*p) strfreeU( *p++ );
        heap_free( strarray );
    }
}

static inline WLDAP32_LDAP *ldap_wrap( LDAP *ld )
{
    if (ld)
    {
        WLDAP32_LDAP *wld = heap_alloc_zero( sizeof(WLDAP32_LDAP) );
        if (!wld)
        {
            ldap_unbind_ext( ld, NULL, NULL );
            return NULL;
        }
        wld->ld_ld64 = ld;
        /* TODO: call libldap info function to fill in ld_host, options, etc?
         * WLDAP32_LDAP is supposed to be opaque but it is documented.
         * However, the normal Wine implementation treats a libldap LDAP as a
         * WLDAP32_LDAP, and the two are not entirely binary compatible.
         * LDAP also varies between libldap versions. */
        return wld;
    }
    return NULL;
}

static inline LDAP *ldap_get( WLDAP32_LDAP *ld )
{
    return ld->ld_ld64;
}

static inline void ldap_unwrap( WLDAP32_LDAP *ld )
{
    heap_free( ld );
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
    ber_bvfree( bv );
    return (bv && !*ptr) ? WLDAP32_LDAP_NO_MEMORY : ret;
}

static inline DWORD bvarraylen( struct berval ** HOSTPTR bv )
{
    struct berval ** HOSTPTR p = bv;
    while (*p) p++;
    return p - bv;
}

static inline struct WLDAP32_berval **bvarrayconvert_h2w( struct berval ** HOSTPTR bv )
{
    struct WLDAP32_berval **bvconv = NULL;

    if (bv)
    {
        DWORD size = sizeof(struct WLDAP32_berval *) * (bvarraylen( bv ) + 1);
        if ((bvconv = heap_alloc( size )))
        {
            struct berval ** HOSTPTR p = bv;
            struct WLDAP32_berval **q = bvconv;

            while (*p) *q++ = bvconvert_h2w( *p++ );
            *q = NULL;
        }
    }
    return bvconv;
}

static inline struct WLDAP32_berval **bvlenconvert_and_free( struct berval ** HOSTPTR values )
{
    struct WLDAP32_berval **ptr = bvarrayconvert_h2w( values );
    ldap_value_free_len( values );
    return ptr;
}

static inline struct WLDAP32_berval **bvecconvert_and_free( struct berval ** HOSTPTR bvec )
{
    struct WLDAP32_berval **ptr = bvarrayconvert_h2w( bvec );
    ber_bvecfree( bvec );
    return ptr;
}

static inline struct berval * WIN32PTR bvconvert_w2h( struct WLDAP32_berval *bv )
{
    struct berval * WIN32PTR bvconv = NULL;

    if (bv)
    {
        DWORD size = sizeof(struct berval) + bv->bv_len;
        if ((bvconv = heap_alloc( size )))
        {
            char *val = (char *)bvconv + sizeof(struct berval);

            bvconv->bv_len = bv->bv_len;
            bvconv->bv_val = val;
            memcpy( val, bv->bv_val, bv->bv_len );
        }
    }
    return bvconv;
}

static inline struct berval **bvarrayconvert( struct WLDAP32_berval **bv )
{
    struct berval **bvconv = NULL;

    if (bv)
    {
        DWORD size = sizeof(struct berval *) * (bvarraylen_w( bv ) + 1);
        if ((bvconv = heap_alloc( size )))
        {
            struct WLDAP32_berval **p = bv;
            struct berval **q = bvconv;

            while (*p) *q++ = bvconvert_w2h( *p++ );
            *q = NULL;
        }
    }
    return bvconv;
}

static inline void bvarrayfree( struct berval ** HOSTPTR bv )
{
    struct berval ** HOSTPTR p = bv;
    while (*p) heap_free( *p++ );
    heap_free( bv );
}

static inline void bvtemparrayfree( struct berval ** HOSTPTR bv )
{
    bvarrayfree( bv );
}

static inline int count_values_len( struct WLDAP32_berval **vals )
{
    return bvarraylen_w( vals );
}

static inline void bvlenfree( struct WLDAP32_berval **bv )
{
    bvarrayfree_w( bv );
}

static inline void bvecfree( struct WLDAP32_berval **bv )
{
    bvarrayfree_w( bv );
}

static inline struct WLDAP32_berval *bvwdup( struct WLDAP32_berval *bv )
{
    return bvdup( bv );
}

static inline void bvfree( struct WLDAP32_berval *bv )
{
    heap_free( bv );
}

static inline ULONG ber_cookie_wrap( ULONG ret, CHAR * HOSTPTR cookie, CHAR **opaque )
{
    CHAR *p = heap_alloc( sizeof(cookie) );
    if (!p) ret = (ULONG)LBER_ERROR;
    else *(CHAR * HOSTPTR *)p = cookie;
    *opaque = p;
    return ret;
}

static inline CHAR * HOSTPTR ber_cookie_get( CHAR *opaque )
{
    return *(CHAR * HOSTPTR *)opaque;
}

static inline void ber_cookie_free( CHAR *opaque )
{
    heap_free( opaque );
}

static inline WLDAP32_BerElement *ber_wrap( BerElement *ber, int buf )
{
    BerElement **ptr = NULL;

    if (ber)
    {
        ptr = heap_alloc( sizeof(ber) );
        if (!ptr) ber_free( ber, buf );
        else *ptr = ber;
    }
    return (WLDAP32_BerElement*)ptr;
}

static inline BerElement *ber_get( WLDAP32_BerElement *ptr )
{
    if (!ptr) return NULL;
    return *(BerElement**)ptr;
}

static inline void ber_unwrap_and_free( WLDAP32_BerElement *ptr, INT buf )
{
    if (ptr)
    {
        ber_free( ber_get( ptr ), buf );
        heap_free( ptr );
    }
}

static inline WLDAP32_LDAPMessage *message_wrap_create( LDAPMessage *msg )
{
    WLDAP32_LDAPMessage *res = heap_alloc_zero( sizeof(WLDAP32_LDAPMessage) );
    if (res) res->lm_msg64 = msg;
    /* TODO: the same applies here as in ldap_wrap() above. */
    return res;
}

static inline ULONG message_wrap( ULONG ret, LDAPMessage *msg, WLDAP32_LDAPMessage **ptr )
{
    WLDAP32_LDAPMessage *res = NULL;

    if (msg && !(res = message_wrap_create( msg )))
    {
        ldap_msgfree( msg );
        ret = WLDAP32_LDAP_NO_MEMORY;
    }
    *ptr = res;
    return ret;
}

static inline LDAPMessage *ldmsg_get( WLDAP32_LDAPMessage *msg )
{
    if (!msg) return NULL;
    return msg->lm_msg64;
}

/* Keep a linked list of messages so they can be freed with the result message. */
static inline WLDAP32_LDAPMessage *message_tail_insert( WLDAP32_LDAPMessage *res, LDAPMessage *msg )
{
    WLDAP32_LDAPMessage *next = NULL;

    if (res && msg)
    {
        next = res;
        while (next->lm_next) next = next->lm_next;
        next->lm_next = message_wrap_create( msg );
    }
    return next;
}

static inline void message_free_chain( WLDAP32_LDAPMessage *res )
{
    WLDAP32_LDAPMessage *msg = res;
    ldap_msgfree( res->lm_msg64 );
    while (msg)
    {
        WLDAP32_LDAPMessage *next = msg->lm_next;
        heap_free( msg );
        msg = next;
    }
}

#else

static inline char *berstrdup_and_free( char * HOSTPTR src )
{
    return src;
}

static inline char **berstrarraydup_and_free( char * HOSTPTR * HOSTPTR strarray )
{
    return strarray;
}

static inline char * HOSTPTR * strhostarray( char ** strarray )
{
    return strarray;
}

static inline void strhostarrayfree( void *ptr )
{
    (void)ptr;
}

static inline WLDAP32_LDAP *ldap_wrap( LDAP *ld )
{
    return ld;
}

static inline LDAP *ldap_get( WLDAP32_LDAP *ld )
{
    return ld;
}

static inline void ldap_unwrap( WLDAP32_LDAP *ld )
{
    (void)ld;
}

static inline struct WLDAP32_berval *bvconvert_h2w( struct berval *bv )
{
    return (struct WLDAP32_berval *)bv;
}

static inline ULONG bvconvert_and_free( ULONG ret, struct berval *bv, struct WLDAP32_berval **ptr )
{
    *ptr = (struct WLDAP32_berval *)bv;
    return ret;
}

static inline struct WLDAP32_berval **bvlenconvert_and_free( struct berval **values )
{
    return (struct WLDAP32_berval **)values;
}

static inline struct WLDAP32_berval **bvecconvert_and_free( struct berval **bvec )
{
    return (struct WLDAP32_berval **)bvec;
}

static inline struct berval *bvconvert_w2h( struct WLDAP32_berval *bv )
{
    return (struct berval *)bv;
}

static inline struct berval **bvarrayconvert( struct WLDAP32_berval **bv )
{
    return (struct berval **)bv;
}

static inline void bvarrayfree( struct berval **bv )
{
    bvarrayfree_w( (struct WLDAP32_berval **)bv );
}

static inline void bvtemparrayfree( struct berval **bv )
{
    (void)bv;
}

static inline int count_values_len( struct WLDAP32_berval **vals )
{
    return ldap_count_values_len( (struct berval **)vals );
}

static inline void bvlenfree( struct WLDAP32_berval **bv )
{
    ldap_value_free_len( (struct berval **)bv );
}

static inline void bvecfree( struct WLDAP32_berval **bv )
{
    ber_bvecfree( (struct berval **)bv );
}

static inline struct WLDAP32_berval *bvwdup( struct WLDAP32_berval *bv )
{
    return (struct WLDAP32_berval *)ber_bvdup( (struct berval *)bv );
}

static inline void bvfree( struct WLDAP32_berval *bv )
{
    ber_bvfree( (struct berval *)bv );
}

static inline ULONG ber_cookie_wrap( ULONG ret, CHAR *cookie, CHAR **opaque )
{
    *opaque = cookie;
    return ret;
}

static inline CHAR *ber_cookie_get( CHAR *opaque )
{
    return opaque;
}

static inline void ber_cookie_free( CHAR *opaque )
{
    (void)opaque;
}

static inline WLDAP32_BerElement *ber_wrap( BerElement *ber, int buf )
{
    (void)buf;
    return (WLDAP32_BerElement*)ber;
}

static inline BerElement *ber_get( WLDAP32_BerElement *ptr )
{
    return ptr;
}

static inline void ber_unwrap_and_free( WLDAP32_BerElement *ptr, INT buf )
{
    ber_free( ptr, buf );
}

static inline ULONG message_wrap( ULONG ret, LDAPMessage *msg, WLDAP32_LDAPMessage **ptr )
{
    *ptr = msg;
    return ret;
}

static inline LDAPMessage *ldmsg_get( WLDAP32_LDAPMessage *msg )
{
    return msg;
}

static inline WLDAP32_LDAPMessage *message_tail_insert( WLDAP32_LDAPMessage *res, LDAPMessage *msg64 )
{
    (void)res;
    return msg64;
}

static inline void message_free_chain( WLDAP32_LDAPMessage *res )
{
    ldap_msgfree( res );
}

#endif /* __i386_on_x86_64__ */

static inline void vlvinfo_convert( WLDAP32_LDAPVLVInfo *info, LDAPVLVInfo *res, struct berval bvtemp[2])
{
    res->ldvlv_version = info->ldvlv_version;
    res->ldvlv_before_count = info->ldvlv_before_count;
    res->ldvlv_after_count = info->ldvlv_after_count;
    res->ldvlv_offset = info->ldvlv_offset;
    res->ldvlv_count = info->ldvlv_count;
    res->ldvlv_attrvalue = NULL;
    if (info->ldvlv_attrvalue)
    {
        res->ldvlv_attrvalue = bvtemp;
        bvtemp[0].bv_len = info->ldvlv_attrvalue->bv_len;
        bvtemp[0].bv_val = info->ldvlv_attrvalue->bv_val;
    }
    res->ldvlv_context = NULL;
    if (info->ldvlv_context)
    {
        res->ldvlv_context = bvtemp + 1;
        bvtemp[1].bv_len = info->ldvlv_context->bv_len;
        bvtemp[1].bv_val = info->ldvlv_context->bv_val;
    }
    res->ldvlv_extradata = info->ldvlv_extradata;
}

static inline LDAPModW *modAtoW( LDAPModA *mod )
{
    LDAPModW *modW;

    if ((modW = heap_alloc( sizeof(LDAPModW) )))
    {
        modW->mod_op = mod->mod_op;
        modW->mod_type = strAtoW( mod->mod_type );

        if (mod->mod_op & LDAP_MOD_BVALUES)
            modW->mod_vals.modv_bvals = bvarraydup( mod->mod_vals.modv_bvals );
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
            modU->mod_vals.modv_bvals = bvarrayconvert( mod->mod_vals.modv_bvals );
        else
            modU->mod_vals.modv_strvals = strarrayWtoU( mod->mod_vals.modv_strvals );
    }
    return modU;
}

static inline void modfreeW( LDAPModW *mod )
{
    if (mod->mod_op & LDAP_MOD_BVALUES)
        bvarrayfree_w( mod->mod_vals.modv_bvals );
    else
        strarrayfreeW( mod->mod_vals.modv_strvals );
    heap_free( mod );
}

static inline void modfreeU( LDAPMod *mod )
{
    if (mod->mod_op & LDAP_MOD_BVALUES)
        bvarrayfree( mod->mod_vals.modv_bvals );
    else
        strarrayfreeU( mod->mod_vals.modv_strvals );
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

    controlW->ldctl_oid = strUtoW( control->ldctl_oid );
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
        strfreeU( control->ldctl_oid );
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

static inline DWORD controlarraylenU( LDAPControl ** HOSTPTR controlarray )
{
    LDAPControl ** HOSTPTR p = controlarray;
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

static inline LDAPControlW **controlarrayUtoW( LDAPControl ** HOSTPTR controlarray )
{
    LDAPControlW **controlarrayW = NULL;
    DWORD size;

    if (controlarray)
    {
        size = sizeof(LDAPControlW*) * (controlarraylenU( controlarray ) + 1);
        if ((controlarrayW = heap_alloc( size )))
        {
            LDAPControl ** HOSTPTR p = controlarray;
            LDAPControlW **q = controlarrayW;

            while (*p) *q++ = controlUtoW( *p++ );
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
        strfreeU( sortkey->attributeType );
        strfreeU( sortkey->orderingRule );
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
