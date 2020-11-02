/*
 * Clipboard related functions
 *
 * Copyright 1994 Martin Ayotte
 *	     1996 Alex Korobka
 *	     1999 Noel Borthwick
 *           2003 Ulrich Czekalla for CodeWeavers
 *           2014 Damjan Jovanovic
 *           2016 Vincent Povirk for CodeWeavers
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

#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "config.h"
#include "wine/port.h"

#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/unicode.h"

#include "android.h"
#include "wine/debug.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(clipboard);

static const WCHAR clipboard_classname[] = {'_','_','w','i','n','e','_','c','l','i','p','b','o','a','r','d','_','m','a','n','a','g','e','r',0};
static DWORD clipboard_thread_id;
static HWND clipboard_hwnd;

static ULONG64 last_clipboard_update;

#define CLIPBOARD_UPDATE_DELAY 2000 /* delay between checks of the Android clipboard */

enum clipboard_manager_messages {
    WM_CLIPDATA_UPDATE = WM_USER,
    WM_UPDATECLIPBOARD,
};

typedef struct android_clipformat {
    UINT cf;
    DWORD flags_mask;
    DWORD flags_value;
    LPCWSTR mimetype;
    HANDLE (*import)(void);
    BOOL available;
} android_clipformat;

static HANDLE import_text(void);

static android_clipformat formats [] = {
    { CF_UNICODETEXT, ANDROID_CLIPDATA_HASTEXT|ANDROID_CLIPDATA_EMPTYTEXT, ANDROID_CLIPDATA_HASTEXT, 0, import_text },
};

static void free_mimetypes( LPWSTR *mimetypes )
{
    int i;

    if (!mimetypes)
        return;

    for (i=0; mimetypes[i]; i++)
        free( mimetypes[i] );

    free( mimetypes );
}

static HANDLE import_text(void)
{
    LPWSTR text = NULL;
    HANDLE result = NULL;
    void *data;

    TRACE("\n");

    ioctl_get_clipdata( ANDROID_CLIPDATA_HASTEXT, &text );

    if (text != NULL)
    {
        int size = sizeof(WCHAR) * (strlenW( text ) + 1);

        result = GlobalAlloc( GMEM_MOVEABLE, size );

        data = GlobalLock( result );

        strcpyW( data, text );

        GlobalUnlock( result );

        HeapFree( GetProcessHeap(), 0, text );
    }

    return result;
}

void CDECL ANDROID_UpdateClipboard(void)
{
    static ULONG last_update;
    ULONG now;
    static HWND clipboard_hwnd;

    if (GetCurrentThreadId() == clipboard_thread_id) return;

    now = GetTickCount();
    if ((int)(now - last_update) <= CLIPBOARD_UPDATE_DELAY) return;

    if (!clipboard_hwnd)
        clipboard_hwnd = FindWindowW( clipboard_classname, NULL );

    SendMessageW( clipboard_hwnd, WM_UPDATECLIPBOARD, 0, 0 );
}

/**************************************************************************
 *		clipboard_wndproc
 *
 * Window procedure for the clipboard manager.
 */
static LRESULT CALLBACK clipboard_wndproc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    switch (msg)
    {
    case WM_NCCREATE:
        return TRUE;
    case WM_CLIPDATA_UPDATE:
    {
        DWORD flags = (DWORD)wp;
        LPWSTR* mimetypes = (LPWSTR*)lp;
        int i;

        TRACE("flags=%x\n", flags);

        if ((flags & ANDROID_CLIPDATA_OURS) && GetClipboardOwner() == NULL)
            /* Wine restarted since copying this data, let's get what we can from it. */
            flags &= ~(ANDROID_CLIPDATA_OURS | ANDROID_CLIPDATA_HASURI);

        if (!(flags & ANDROID_CLIPDATA_OURS))
        {
            if (TRACE_ON(clipboard))
            {
                TRACE("mimetypes:");
                if (mimetypes)
                {
                    for (i=0; mimetypes[i]; i++)
                        TRACE(" %s", debugstr_w(mimetypes[i]));
                    TRACE("\n");
                }
                else
                    TRACE(" (null)\n");
            }

            OpenClipboard( hwnd );

            EmptyClipboard();

            for (i=0; i<sizeof(formats)/sizeof(formats[0]); i++)
            {
                formats[i].available = FALSE;

                if (IsClipboardFormatAvailable( formats[i].cf ))
                    continue;

                if ((flags & formats[i].flags_mask) != formats[i].flags_value)
                    continue;

                if (formats[i].mimetype != NULL)
                {
                    int j;
                    BOOL found = FALSE;
                    for (j=0; mimetypes[j]; j++)
                    {
                        if (strcmpiW( mimetypes[j], formats[i].mimetype ) == 0)
                        {
                            found = TRUE;
                            break;
                        }
                    }
                    if (!found)
                        continue;
                }

                SetClipboardData( formats[i].cf, NULL );
                formats[i].available = TRUE;
            }

            CloseClipboard();
        }

        last_clipboard_update = GetTickCount64();

        free_mimetypes(mimetypes);

        break;
    }
    case WM_RENDERFORMAT:
    {
        int i;
        UINT cf = (UINT)wp;

        for (i=0; i<sizeof(formats)/sizeof(formats[0]); i++)
        {
            if (formats[i].cf == cf && formats[i].available)
            {
                SetClipboardData( cf, formats[i].import() );

                return 0;
            }
        }

        break;
    }
    case WM_CLIPBOARDUPDATE:
    {
        HANDLE handle;
        LPWSTR text = NULL;

        TRACE("WM_CLIPBOARDUPATE\n");

        OpenClipboard( hwnd );

        if (GetClipboardOwner() == clipboard_hwnd)
        {
            CloseClipboard();
            break;
        }

        handle = GetClipboardData( CF_UNICODETEXT );

        if (handle != NULL)
        {
            int size = GlobalSize( handle );
            void* data = GlobalLock( handle );

            text = HeapAlloc( GetProcessHeap(), 0, size + 2 );

            memcpy( text, data, size );
            text[ size / 2 ] = 0;

            GlobalUnlock( handle );
        }

        CloseClipboard();

        ioctl_set_clipdata( text );

        HeapFree( GetProcessHeap(), 0, text );

        break;
    }
    case WM_UPDATECLIPBOARD:
    {
        MSG m;

        if (GetTickCount64() - last_clipboard_update <= CLIPBOARD_UPDATE_DELAY) break;

        ioctl_poll_clipdata();

        GetMessageW( &m, clipboard_hwnd, WM_CLIPDATA_UPDATE, WM_CLIPDATA_UPDATE );
        DispatchMessageW( &m );

        break;
    }
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

/**************************************************************************
 *		wait_clipboard_mutex
 *
 * Make sure that there's only one clipboard thread per window station.
 */
static BOOL wait_clipboard_mutex(void)
{
    static const WCHAR prefix[] = {'_','_','w','i','n','e','_','c','l','i','p','b','o','a','r','d','_'};
    WCHAR buffer[MAX_PATH + sizeof(prefix) / sizeof(WCHAR)];
    HANDLE mutex;

    memcpy( buffer, prefix, sizeof(prefix) );
    if (!GetUserObjectInformationW( GetProcessWindowStation(), UOI_NAME,
                                    buffer + sizeof(prefix) / sizeof(WCHAR),
                                    sizeof(buffer) - sizeof(prefix), NULL ))
    {
        ERR( "failed to get winstation name\n" );
        return FALSE;
    }
    mutex = CreateMutexW( NULL, TRUE, buffer );
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        TRACE( "waiting for mutex %s\n", debugstr_w( buffer ));
        WaitForSingleObject( mutex, INFINITE );
    }
    return TRUE;
}

/**************************************************************************
 *		clipboard_thread
 *
 * Thread running inside the desktop process to manage the clipboard
 */
static DWORD WINAPI clipboard_thread( void *arg )
{
    WNDCLASSW class;
    MSG msg;

    if (!wait_clipboard_mutex()) return 0;

    memset( &class, 0, sizeof(class) );
    class.lpfnWndProc   = clipboard_wndproc;
    class.lpszClassName = clipboard_classname;

    if (!RegisterClassW( &class ) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        ERR( "could not register clipboard window class err %u\n", GetLastError() );
        return 0;
    }
    if (!(clipboard_hwnd = CreateWindowW( clipboard_classname, NULL, 0, 0, 0, 0, 0,
                                          HWND_MESSAGE, 0, 0, NULL )))
    {
        ERR( "failed to create clipboard window err %u\n", GetLastError() );
        return 0;
    }

    clipboard_thread_id = GetCurrentThreadId();

    ioctl_poll_clipdata();

    AddClipboardFormatListener( clipboard_hwnd );

    TRACE( "clipboard thread %04x running\n", GetCurrentThreadId() );
    while (GetMessageW( &msg, 0, 0, 0 )) DispatchMessageW( &msg );
    return 0;
}

void handle_clipdata_update( DWORD flags, LPWSTR *mimetypes )
{
    if (clipboard_hwnd == NULL)
    {
        WARN( "no clipboard manager window\n" );
        free_mimetypes( mimetypes );
        return;
    }

    PostMessageW( clipboard_hwnd, WM_CLIPDATA_UPDATE, flags, (LPARAM)mimetypes );
}

void init_clipboard(void)
{
    DWORD id;
    HANDLE handle = CreateThread( NULL, 0, clipboard_thread, NULL, 0, &id );

    if (handle) CloseHandle( handle );
    else ERR( "failed to create clipboard thread\n" );
}
