/*
 * Mac graphics driver hooks for Apple Game Porting Toolkit D3D layer
 *
 * Copyright 2023 Brendan Shanks for CodeWeavers, Inc.
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

#if defined(__x86_64__)

#include "config.h"

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "macdrv.h"
#include "shellapi.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(macdrv_gpt);

typedef LONG LSTATUS;

struct macdrv_functions_t
{
    void (*macdrv_init_display_devices)(BOOL);
    struct macdrv_win_data*(*get_win_data)(HWND hwnd);
    void (*release_win_data)(struct macdrv_win_data *data);
    macdrv_window(*macdrv_get_cocoa_window)(HWND hwnd, BOOL require_on_screen);
    macdrv_metal_device (*macdrv_create_metal_device)(void);
    void (*macdrv_release_metal_device)(macdrv_metal_device d);
    macdrv_metal_view (*macdrv_view_create_metal_view)(macdrv_view v, macdrv_metal_device d);
    macdrv_metal_layer (*macdrv_view_get_metal_layer)(macdrv_metal_view v);
    void (*macdrv_view_release_metal_view)(macdrv_metal_view v);
    void (*on_main_thread)(dispatch_block_t b);
    LSTATUS(WINAPI*RegQueryValueExA)(HKEY, LPCSTR, LPDWORD, LPDWORD, BYTE*, LPDWORD);
    LSTATUS(WINAPI*RegSetValueExA)(HKEY, LPCSTR, DWORD, DWORD, const BYTE*, DWORD);
    LSTATUS(WINAPI*RegOpenKeyExA)(HKEY, LPCSTR, DWORD, DWORD, HKEY*);
    LSTATUS(WINAPI*RegCreateKeyExA)(HKEY, LPCSTR, DWORD, LPSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, HKEY*, LPDWORD);
    LSTATUS(WINAPI*RegCloseKey)(HKEY);
    BOOL(WINAPI*EnumDisplayMonitors)(HDC,LPRECT,MONITORENUMPROC,LPARAM);
    BOOL(WINAPI*GetMonitorInfoA)(HMONITOR,LPMONITORINFO);
    BOOL(WINAPI*AdjustWindowRectEx)(LPRECT,DWORD,BOOL,DWORD);
    LONG_PTR(WINAPI*GetWindowLongPtrW)(HWND,int);
    BOOL(WINAPI*GetWindowRect)(HWND,LPRECT);
    BOOL(WINAPI*MoveWindow)(HWND,int,int,int,int,BOOL);
    BOOL(WINAPI*SetWindowPos)(HWND,HWND,int,int,int,int,UINT);
    INT(WINAPI*GetSystemMetrics)(INT);
    LONG_PTR(WINAPI*SetWindowLongPtrW)(HWND,INT,LONG_PTR);
};
C_ASSERT(sizeof(struct macdrv_functions_t) == 192);
C_ASSERT(sizeof(struct macdrv_win_data) == 120);

void OnMainThread(dispatch_block_t block);

static void my_macdrv_init_display_devices(BOOL p1)
{
    TRACE("macdrv_init_display_devices %d\n", p1);
    macdrv_init_display_devices(p1);
}

static struct macdrv_win_data *my_get_win_data(HWND hwnd)
{
    TRACE("get_win_data %p\n", hwnd);
    return get_win_data(hwnd);
}

static void my_release_win_data(struct macdrv_win_data *data)
{
    TRACE("release_win_data %p\n", data);
    release_win_data(data);
}

static macdrv_window my_macdrv_get_cocoa_window(HWND hwnd, BOOL require_on_screen)
{
    TRACE("macdrv_get_cocoa_window %p %d\n", hwnd, require_on_screen);
    return macdrv_get_cocoa_window(hwnd, require_on_screen);
}

static macdrv_metal_device my_macdrv_create_metal_device(void)
{
    TRACE("macdrv_create_metal_device\n");
    return macdrv_create_metal_device();
}

static void my_macdrv_release_metal_device(macdrv_metal_device d)
{
    TRACE("macdrv_release_metal_device %p\n", d);
    macdrv_release_metal_device(d);
}

static macdrv_metal_view my_macdrv_view_create_metal_view(macdrv_view v, macdrv_metal_device d)
{
    TRACE("macdrv_view_create_metal_view %p %p\n", v, d);
    return macdrv_view_create_metal_view(v, d);
}

static macdrv_metal_layer my_macdrv_view_get_metal_layer(macdrv_metal_view v)
{
    TRACE("macdrv_view_get_metal_layer %p\n", v);
    return macdrv_view_get_metal_layer(v);
}

static void my_macdrv_view_release_metal_view(macdrv_metal_view v)
{
    TRACE("macdrv_view_release_metal_view %p\n", v);
    return macdrv_view_release_metal_view(v);
}

static void my_OnMainThread(dispatch_block_t b)
{
    TRACE("OnMainThread %p\n", b);
    OnMainThread(b);
}


static LSTATUS WINAPI my_RegQueryValueExA(HKEY p1, LPCSTR p2, LPDWORD p3, LPDWORD p4, BYTE* p5, LPDWORD p6)
{
    LSTATUS result;
    struct regqueryvalueexa_params params =
    {
        .hkey = HandleToUlong(p1),
        .name = (UINT_PTR)p2,
        .reserved = (UINT_PTR)p3,
        .type = (UINT_PTR)p4,
        .data = (UINT_PTR)p5,
        .count = (UINT_PTR)p6,
        .result = (UINT_PTR)&result,
    };

    TRACE("RegQueryValueExA %p %s %p %p %p %p\n", p1, p2, p3, p4, p5, p6);

    macdrv_client_func(client_func_regqueryvalueexa, &params, sizeof(params));

    return result;
}

static LSTATUS WINAPI my_RegSetValueExA(HKEY p1, LPCSTR p2, DWORD p3, DWORD p4, const BYTE* p5, DWORD p6)
{
    LSTATUS result;
    struct regsetvalueexa_params params =
    {
        .hkey = HandleToUlong(p1),
        .name = (UINT_PTR)p2,
        .reserved = p3,
        .type = p4,
        .data = (UINT_PTR)p5,
        .count = p6,
        .result = (UINT_PTR)&result,
    };

    TRACE("RegSetValueExA %p %s(%p) %d %p %d\n", p1, p2, p2, p4, p5, p6);

    macdrv_client_func(client_func_regsetvalueexa, &params, sizeof(params));

    return result;
}

static LSTATUS WINAPI my_RegOpenKeyExA(HKEY p1, LPCSTR p2, DWORD p3, DWORD p4, HKEY* p5)
{
    LSTATUS result;
    struct regcreateopenkeyexa_params params =
    {
        .create = 0,
        .hkey = HandleToUlong(p1),
        .name = (UINT_PTR)p2,
        .options = p3,
        .access = p4,
        .retkey = (UINT_PTR)p5,
        .result = (UINT_PTR)&result,
    };

    TRACE("RegOpenKeyExA %p %s\n", p1, p2);

    macdrv_client_func(client_func_regcreateopenkeyexa, &params, sizeof(params));

    return result;
}

static LSTATUS WINAPI my_RegCreateKeyExA(HKEY p1, LPCSTR p2, DWORD p3, LPSTR p4, DWORD p5, DWORD p6, LPSECURITY_ATTRIBUTES p7, HKEY* p8, LPDWORD p9)
{
    LSTATUS result;
    struct regcreateopenkeyexa_params params =
    {
        .create = 1,
        .hkey = HandleToUlong(p1),
        .name = (UINT_PTR)p2,
        .reserved = p3,
        .class = (UINT_PTR)p4,
        .options = p5,
        .access = p6,
        .security = (UINT_PTR)p7,
        .retkey = (UINT_PTR)p8,
        .disposition = (UINT_PTR)p9,
        .result = (UINT_PTR)&result,
    };

    TRACE("RegCreateKeyExA %p %s\n", p1, p2);

    macdrv_client_func(client_func_regcreateopenkeyexa, &params, sizeof(params));

    return result;
}

static LSTATUS WINAPI DECLSPEC_HOTPATCH RegCloseKey( HKEY hkey )
{
    if (!hkey) return ERROR_INVALID_HANDLE;
    if (hkey >= (HKEY)0x80000000) return ERROR_SUCCESS;
    return RtlNtStatusToDosError( NtClose( hkey ) );
}

static LSTATUS WINAPI my_RegCloseKey(HKEY hkey)
{
    TRACE("RegCloseKey %p\n", hkey);
    return RegCloseKey(hkey);
}

static BOOL WINAPI my_EnumDisplayMonitors(HDC h, LPRECT p2, MONITORENUMPROC p3, LPARAM p4)
{
    TRACE("EnumDisplayMonitors %p %p %p %ld\n", h, p2, p3, p4);
    return NtUserEnumDisplayMonitors(h, p2, p3, p4);
}

static BOOL WINAPI my_GetMonitorInfoA(HMONITOR monitor, LPMONITORINFO info)
{
    MONITORINFOEXW miW;
    BOOL ret;

    TRACE("GetMonitorInfoA %p %p\n", monitor, info);

    if (info->cbSize == sizeof(MONITORINFO)) return NtUserGetMonitorInfo( monitor, info );
    if (info->cbSize != sizeof(MONITORINFOEXA)) return FALSE;

    miW.cbSize = sizeof(miW);
    ret = NtUserGetMonitorInfo( monitor, (MONITORINFO *)&miW );
    if (ret)
    {
        MONITORINFOEXA *miA = (MONITORINFOEXA *)info;
        ULONG size;
        miA->rcMonitor = miW.rcMonitor;
        miA->rcWork = miW.rcWork;
        miA->dwFlags = miW.dwFlags;
        RtlUnicodeToUTF8N(miA->szDevice, sizeof(miA->szDevice), &size, miW.szDevice, lstrlenW(miW.szDevice) * sizeof(WCHAR));
    }
    return ret;
}

static BOOL WINAPI my_AdjustWindowRectEx(LPRECT p1,DWORD p2,BOOL p3,DWORD p4)
{
    TRACE("AdjustWindowRectEx %p %u %d %u\n", p1, p2, p3, p4);
    return AdjustWindowRectEx(p1, p2, p3, p4);
}

static LONG_PTR WINAPI my_GetWindowLongPtrW(HWND h,int nIndex)
{
    TRACE("GetWindowLongPtrW %p\n", h);
    /* ignore possibility of DWLP_DLGPROC */
    return NtUserGetWindowLongPtrW(h, nIndex);
}

static BOOL WINAPI my_GetWindowRect(HWND h, LPRECT rect)
{
    TRACE("GetWindowRect %p %p\n", h, rect);
    return NtUserGetWindowRect(h, rect);
}

static BOOL WINAPI my_MoveWindow(HWND h, int X,int Y,int nWidth,int nHeight,BOOL bRepaint)
{
    TRACE("MoveWindow %p %d %d %d %d %d\n", h, X, Y, nWidth, nHeight, bRepaint);
    return NtUserMoveWindow(h, X, Y, nWidth, nHeight, bRepaint);
}

static BOOL WINAPI my_SetWindowPos(HWND h,HWND h2,int x,int y,int cx,int cy,UINT flags)
{
    TRACE("SetWindowPos %p %p %d %d %d %d %u\n", h, h2, x, y, cx, cy, flags);
    return NtUserSetWindowPos(h, h2, x, y, cx, cy, flags);
}

static INT WINAPI my_GetSystemMetrics(INT index)
{
    TRACE("GetSystemMetrics %d\n", index);
    return NtUserGetSystemMetrics( index );
}

static LONG_PTR WINAPI my_SetWindowLongPtrW(HWND hwnd, INT offset, LONG_PTR newval)
{
    TRACE("SetWindowLongPtrW %p %d %ld\n", hwnd, offset, newval);
    /* ignore possibility of DWLP_DLGPROC */
    return NtUserSetWindowLongPtr( hwnd, offset, newval, FALSE );
}

struct macdrv_functions_t macdrv_functions = {
    &my_macdrv_init_display_devices,
    &my_get_win_data,
    &my_release_win_data,
    &my_macdrv_get_cocoa_window,
    &my_macdrv_create_metal_device,
    &my_macdrv_release_metal_device,
    &my_macdrv_view_create_metal_view,
    &my_macdrv_view_get_metal_layer,
    &my_macdrv_view_release_metal_view,
    &my_OnMainThread,
    &my_RegQueryValueExA,
    &my_RegSetValueExA,
    &my_RegOpenKeyExA,
    &my_RegCreateKeyExA,
    &my_RegCloseKey,
    &my_EnumDisplayMonitors,
    &my_GetMonitorInfoA,
    &my_AdjustWindowRectEx,
    &my_GetWindowLongPtrW,
    &my_GetWindowRect,
    &my_MoveWindow,
    &my_SetWindowPos,
    &my_GetSystemMetrics,
    &my_SetWindowLongPtrW,
};

#endif
