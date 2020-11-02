/*
 * Default entry point for an exe
 *
 * Copyright 2005 Alexandre Julliard
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

#if 0
#pragma makedep unix
#endif

#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/library.h"
#include "wine/heap.h"
#include "crt0_private.h"

extern int __cdecl main( int argc, char *argv[] );

DWORD WINAPI DECLSPEC_HIDDEN __wine_spec_exe_entry( PEB *peb )
{
    BOOL needs_init = (__wine_spec_init_state != CONSTRUCTORS_DONE);
    DWORD ret;
    char **argv;

#ifdef __i386_on_x86_64__
    char *p;
    size_t len = 0;
    for (int i = 0; i < __wine_main_argc; ++i)
        len += strlen(__wine_main_argv[i]) + 1;
    argv = heap_alloc((__wine_main_argc + 1) * sizeof(*argv) + len);
    p = (char*)argv + (__wine_main_argc + 1) * sizeof(*argv);
    for (int i = 0; i < __wine_main_argc; ++i)
    {
        argv[i] = p;
        p = stpcpy(p, __wine_main_argv[i]) + 1;
    }
    argv[__wine_main_argc] = NULL;
#else
    argv = __wine_main_argv;
#endif

    if (needs_init) _init( __wine_main_argc, __wine_main_argv, NULL );
    ret = main( __wine_main_argc, argv );
#ifdef __i386_on_x86_64__
    heap_free(argv);
#endif
    if (needs_init) _fini();
    ExitProcess( ret );
}
