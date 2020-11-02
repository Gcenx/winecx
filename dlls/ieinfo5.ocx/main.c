/*
 * Copyright 2003 Mike McCormack for CodeWeavers
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

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"
#include "winuser.h"

#include "ole2.h"
#include "olectl.h"

#include "shlwapi.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ieinfo);

HRESULT WINAPI DllGetVersion(DLLVERSIONINFO *pdvi)
{
    TRACE("%p\n",pdvi);

    if (pdvi->cbSize != sizeof(DLLVERSIONINFO))
        return E_INVALIDARG;

    pdvi->dwMajorVersion = 5;
    pdvi->dwMinorVersion = 0;
    pdvi->dwBuildNumber = 0;
    pdvi->dwPlatformID = 1;

    return S_OK;
}

HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID iid,LPVOID *ppv)
{
    FIXME("stub\n");
    return CLASS_E_CLASSNOTAVAILABLE;
}


/***********************************************************************
 *		DllRegisterServer (IEINFO5.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    FIXME("stub\n");
    return S_OK;
}

/***********************************************************************
 *		DllUnregisterServer (IEINFO5.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    FIXME("stub\n");
    return S_OK;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

