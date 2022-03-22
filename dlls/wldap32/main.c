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

#include "wine/debug.h"
#include <stdarg.h>
#if defined(HAVE_LDAP) && !defined(__i386_on_x86_64__)
# include <dlfcn.h>
# include <ldap.h>
#endif

#include "windef.h"
#include "winbase.h"
#include "winldap_private.h"
#include "wldap32.h"

HINSTANCE hwldap32;

WINE_DEFAULT_DEBUG_CHANNEL(wldap32);
#if defined(HAVE_LDAP) && !defined(__i386_on_x86_64__)
WINE_DECLARE_DEBUG_CHANNEL(winediag);

static void *liblber_handle;
static void *libldap_handle;

#define MAKE_FUNCPTR(f) typeof(f) * p##f
MAKE_FUNCPTR(ber_alloc_t);
MAKE_FUNCPTR(ber_bvfree);
MAKE_FUNCPTR(ber_first_element);
MAKE_FUNCPTR(ber_flatten);
MAKE_FUNCPTR(ber_free);
MAKE_FUNCPTR(ber_init);
MAKE_FUNCPTR(ber_next_element);
MAKE_FUNCPTR(ber_peek_tag);
MAKE_FUNCPTR(ber_skip_tag);
MAKE_FUNCPTR(ber_printf);
MAKE_FUNCPTR(ber_scanf);
#undef MAKE_FUNCPTR

/* CrossOver hack - bug 19582 */
static BOOL load_liblber(void)
{
    int i;
    const char *candidates[] = { "liblber-2.4.so.2", "liblber-2.5.so.0", "liblber.so.2", "liblber.dylib", NULL };

    for (i = 0; candidates[i]; i++)
    {
        if ((liblber_handle = dlopen( candidates[i], RTLD_NOW ))) break;
    }

    if (!liblber_handle)
    {
        ERR_(winediag)( "failed to load liblber\n" );
        return FALSE;
    }

#define LOAD_FUNCPTR(f) \
    if (!(p##f = dlsym( liblber_handle, #f ))) \
    { \
        ERR( "failed to load %s\n", #f ); \
        goto fail; \
    }

    LOAD_FUNCPTR(ber_alloc_t)
    LOAD_FUNCPTR(ber_bvfree)
    LOAD_FUNCPTR(ber_first_element)
    LOAD_FUNCPTR(ber_flatten)
    LOAD_FUNCPTR(ber_free)
    LOAD_FUNCPTR(ber_init)
    LOAD_FUNCPTR(ber_next_element)
    LOAD_FUNCPTR(ber_peek_tag)
    LOAD_FUNCPTR(ber_skip_tag)
    LOAD_FUNCPTR(ber_printf)
    LOAD_FUNCPTR(ber_scanf)
#undef LOAD_FUNCPTR
    return TRUE;

fail:
    dlclose( liblber_handle );
    liblber_handle = NULL;
    return FALSE;
}

#define MAKE_FUNCPTR(f) typeof(f) * p##f
MAKE_FUNCPTR(ldap_abandon_ext);
MAKE_FUNCPTR(ldap_add_ext);
MAKE_FUNCPTR(ldap_add_ext_s);
MAKE_FUNCPTR(ldap_compare_ext);
MAKE_FUNCPTR(ldap_compare_ext_s);
MAKE_FUNCPTR(ldap_control_free);
MAKE_FUNCPTR(ldap_controls_free);
MAKE_FUNCPTR(ldap_count_entries);
MAKE_FUNCPTR(ldap_count_references);
MAKE_FUNCPTR(ldap_count_values_len);
MAKE_FUNCPTR(ldap_create_sort_control);
MAKE_FUNCPTR(ldap_create_vlv_control);
MAKE_FUNCPTR(ldap_delete_ext);
MAKE_FUNCPTR(ldap_delete_ext_s);
MAKE_FUNCPTR(ldap_dn2ufn);
MAKE_FUNCPTR(ldap_explode_dn);
MAKE_FUNCPTR(ldap_extended_operation);
MAKE_FUNCPTR(ldap_extended_operation_s);
MAKE_FUNCPTR(ldap_first_attribute);
MAKE_FUNCPTR(ldap_first_entry);
MAKE_FUNCPTR(ldap_first_reference);
MAKE_FUNCPTR(ldap_get_dn);
MAKE_FUNCPTR(ldap_get_option);
MAKE_FUNCPTR(ldap_get_values_len);
MAKE_FUNCPTR(ldap_initialize);
MAKE_FUNCPTR(ldap_memfree);
MAKE_FUNCPTR(ldap_memvfree);
MAKE_FUNCPTR(ldap_modify_ext);
MAKE_FUNCPTR(ldap_modify_ext_s);
MAKE_FUNCPTR(ldap_msgfree);
MAKE_FUNCPTR(ldap_next_attribute);
MAKE_FUNCPTR(ldap_next_entry);
MAKE_FUNCPTR(ldap_next_reference);
MAKE_FUNCPTR(ldap_parse_extended_result);
MAKE_FUNCPTR(ldap_parse_reference);
MAKE_FUNCPTR(ldap_parse_result);
MAKE_FUNCPTR(ldap_parse_sortresponse_control);
MAKE_FUNCPTR(ldap_parse_vlvresponse_control);
MAKE_FUNCPTR(ldap_rename);
MAKE_FUNCPTR(ldap_rename_s);
MAKE_FUNCPTR(ldap_result);
MAKE_FUNCPTR(ldap_sasl_bind);
MAKE_FUNCPTR(ldap_sasl_bind_s);
MAKE_FUNCPTR(ldap_sasl_interactive_bind_s);
MAKE_FUNCPTR(ldap_search_ext);
MAKE_FUNCPTR(ldap_search_ext_s);
MAKE_FUNCPTR(ldap_set_option);
MAKE_FUNCPTR(ldap_start_tls_s);
MAKE_FUNCPTR(ldap_unbind_ext);
MAKE_FUNCPTR(ldap_unbind_ext_s);
MAKE_FUNCPTR(ldap_value_free_len);
#undef MAKE_FUNCPTR

static BOOL load_libldap(void)
{
    int i;
    const char *candidates[] = { "libldap_r-2.4.so.2", "libldap-2.5.so.0", "libldap.so.2", "libldap.dylib", NULL };

    for (i = 0; candidates[i]; i++)
    {
        if ((libldap_handle = dlopen( candidates[i], RTLD_NOW ))) break;
    }

    if (!libldap_handle)
    {
        ERR_(winediag)( "failed to load libldap\n" );
        return FALSE;
    }

#define LOAD_FUNCPTR(f) \
    if (!(p##f = dlsym( libldap_handle, #f ))) \
    { \
        ERR( "failed to load %s\n", #f ); \
        goto fail; \
    }

    LOAD_FUNCPTR(ldap_abandon_ext)
    LOAD_FUNCPTR(ldap_add_ext)
    LOAD_FUNCPTR(ldap_add_ext_s)
    LOAD_FUNCPTR(ldap_compare_ext)
    LOAD_FUNCPTR(ldap_compare_ext_s)
    LOAD_FUNCPTR(ldap_control_free)
    LOAD_FUNCPTR(ldap_controls_free)
    LOAD_FUNCPTR(ldap_count_entries)
    LOAD_FUNCPTR(ldap_count_references)
    LOAD_FUNCPTR(ldap_count_values_len)
    LOAD_FUNCPTR(ldap_create_sort_control)
    LOAD_FUNCPTR(ldap_create_vlv_control)
    LOAD_FUNCPTR(ldap_delete_ext)
    LOAD_FUNCPTR(ldap_delete_ext_s)
    LOAD_FUNCPTR(ldap_dn2ufn)
    LOAD_FUNCPTR(ldap_explode_dn)
    LOAD_FUNCPTR(ldap_extended_operation)
    LOAD_FUNCPTR(ldap_extended_operation_s)
    LOAD_FUNCPTR(ldap_first_attribute)
    LOAD_FUNCPTR(ldap_first_entry)
    LOAD_FUNCPTR(ldap_first_reference)
    LOAD_FUNCPTR(ldap_get_dn)
    LOAD_FUNCPTR(ldap_get_option)
    LOAD_FUNCPTR(ldap_get_values_len)
    LOAD_FUNCPTR(ldap_initialize)
    LOAD_FUNCPTR(ldap_memfree)
    LOAD_FUNCPTR(ldap_memvfree)
    LOAD_FUNCPTR(ldap_modify_ext)
    LOAD_FUNCPTR(ldap_modify_ext_s)
    LOAD_FUNCPTR(ldap_msgfree)
    LOAD_FUNCPTR(ldap_next_attribute)
    LOAD_FUNCPTR(ldap_next_entry)
    LOAD_FUNCPTR(ldap_next_reference)
    LOAD_FUNCPTR(ldap_parse_extended_result)
    LOAD_FUNCPTR(ldap_parse_reference)
    LOAD_FUNCPTR(ldap_parse_result)
    LOAD_FUNCPTR(ldap_parse_sortresponse_control)
    LOAD_FUNCPTR(ldap_parse_vlvresponse_control)
    LOAD_FUNCPTR(ldap_rename)
    LOAD_FUNCPTR(ldap_rename_s)
    LOAD_FUNCPTR(ldap_result)
    LOAD_FUNCPTR(ldap_sasl_bind)
    LOAD_FUNCPTR(ldap_sasl_bind_s)
    LOAD_FUNCPTR(ldap_sasl_interactive_bind_s)
    LOAD_FUNCPTR(ldap_search_ext)
    LOAD_FUNCPTR(ldap_search_ext_s)
    LOAD_FUNCPTR(ldap_set_option)
    LOAD_FUNCPTR(ldap_start_tls_s)
    LOAD_FUNCPTR(ldap_unbind_ext)
    LOAD_FUNCPTR(ldap_unbind_ext_s)
    LOAD_FUNCPTR(ldap_value_free_len)
#undef LOAD_FUNCPTR
    return TRUE;

fail:
    dlclose( libldap_handle );
    libldap_handle = NULL;
    return FALSE;
}
#endif

BOOL WINAPI DllMain( HINSTANCE hinst, DWORD reason, LPVOID reserved )
{
    TRACE( "(%p, %d, %p)\n", hinst, reason, reserved );

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
#if defined(HAVE_LDAP) && !defined(__i386_on_x86_64__)
        if (!load_liblber() || !load_libldap()) return STATUS_DLL_NOT_FOUND;
#endif
        hwldap32 = hinst;
        DisableThreadLibraryCalls( hinst );
        break;
    case DLL_PROCESS_DETACH:
#if defined(HAVE_LDAP) && !defined(__i386_on_x86_64__)
        dlclose( libldap_handle );
        libldap_handle = NULL;
        dlclose( liblber_handle );
        liblber_handle = NULL;
#endif
        break;
    }
    return TRUE;
}
