/*
 * Joystick functions using the Graphics Driver
 *
 * Copyright 2015 Aric Stewart
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
 *
 */

#include "config.h"
#include "wine/port.h"

#ifndef HAVE_LINUX_JOYSTICK_H
#if !defined(HAVE_IOKIT_HID_IOHIDLIB_H)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "joystick.h"

#include "winbase.h"
#include "winreg.h"
#include "wingdi.h"
#include "winnls.h"
#include "winternl.h"
#include "wine/debug.h"

#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(joystick);

#define MAXJOYSTICK (JOYSTICKID2 + 30)

typedef struct tagGAMEPAD_DRIVER {
    INT (CDECL *pGamePadCount)(void);
    VOID (CDECL *pGamePadName)(int id, char *name, int length);
    VOID (CDECL *pGamePadElementCount)(int id, DWORD *axis, DWORD *buttons, DWORD* povs, int axis_map[8]);
    VOID (CDECL *pGamePadElementProps)(int id, int element, int *min, int *max);
    VOID (CDECL *pGamePadPollValues)(int id, int *values);
    BOOL (CDECL *pGamePadAlloc)(int id);
    VOID (CDECL *pGamePadDealloc)(int id);
} GAMEPAD_DRIVER;

static GAMEPAD_DRIVER null_driver, lazy_load_driver;

const GAMEPAD_DRIVER *GAMEPAD_Driver = &lazy_load_driver;

enum { DRIVER_AXIS_X = 0,
       DRIVER_AXIS_Y,
       DRIVER_AXIS_Z,
       DRIVER_AXIS_RX,
       DRIVER_AXIS_RY,
       DRIVER_AXIS_RZ,
       NUM_AXES};

struct axis {
    int min_value, max_value;
    int value_index;
};

typedef struct tagWINE_JSTCK {
    int   joyIntf;
    BOOL  in_use;

    DWORD nrOfAxes;
    DWORD nrOfButtons;
    DWORD nrOfPOVs;
    struct axis   axes[NUM_AXES];
    int   values[512];
} WINE_JSTCK;

static  WINE_JSTCK  JSTCK_Data[MAXJOYSTICK];

/**************************************************************************
 *  JSTCK_drvGet    [internal]
 */
static WINE_JSTCK *JSTCK_drvGet(DWORD_PTR dwDevID)
{
    int	p;

    if ((dwDevID - (DWORD_PTR)JSTCK_Data) % sizeof(JSTCK_Data[0]) != 0)
        return NULL;
    p = (dwDevID - (DWORD_PTR)JSTCK_Data) / sizeof(JSTCK_Data[0]);
    if (p < 0 || p >= MAXJOYSTICK || !((WINE_JSTCK*)dwDevID)->in_use)
        return NULL;

    return (WINE_JSTCK*)dwDevID;
}

/**************************************************************************
 *  driver_open
 */
LRESULT driver_open(LPSTR str, DWORD dwIntf)
{
    if (dwIntf >= MAXJOYSTICK || JSTCK_Data[dwIntf].in_use)
        return 0;
    if (dwIntf >= GAMEPAD_Driver->pGamePadCount())
        return 0;

    if (!GAMEPAD_Driver->pGamePadAlloc(dwIntf))
    {
        WARN("Driver gamepad alloc failed\n");
        return 0;
    }
    JSTCK_Data[dwIntf].joyIntf = dwIntf;
    JSTCK_Data[dwIntf].in_use = TRUE;
    return (LRESULT)&JSTCK_Data[dwIntf];
}

/**************************************************************************
 *  driver_close
 */
LRESULT driver_close(DWORD_PTR dwDevID)
{
    WINE_JSTCK* jstck = JSTCK_drvGet(dwDevID);

    if (jstck == NULL)
        return 0;
    GAMEPAD_Driver->pGamePadDealloc(jstck->joyIntf);
    jstck->in_use = FALSE;
    return 1;
}

/**************************************************************************
 * JoyGetDevCaps    [MMSYSTEM.102]
 */
LRESULT driver_joyGetDevCaps(DWORD_PTR dwDevID, LPJOYCAPSW lpCaps, DWORD dwSize)
{
    WINE_JSTCK* jstck;
    char    identString[MAXPNAMELEN];
    int     i;
    int     axesMap[8];

    if ((jstck = JSTCK_drvGet(dwDevID)) == NULL)
        return MMSYSERR_NODRIVER;

    GAMEPAD_Driver->pGamePadElementCount(jstck->joyIntf, &jstck->nrOfAxes,
        &jstck->nrOfButtons, &jstck->nrOfPOVs, axesMap);
    GAMEPAD_Driver->pGamePadName(jstck->joyIntf, identString, MAXPNAMELEN);
    TRACE("Name: %s, #Axes: %d, #Buttons: %d\n",
      identString, jstck->nrOfAxes, jstck->nrOfButtons);

    memset(jstck->values, 0, sizeof(jstck->values));

    lpCaps->wMid = MM_MICROSOFT;
    lpCaps->wPid = MM_PC_JOYSTICK;
    MultiByteToWideChar(CP_UNIXCP, 0, identString, -1, lpCaps->szPname, MAXPNAMELEN);
    lpCaps->szPname[MAXPNAMELEN-1] = '\0';
    lpCaps->wXmin = 0;
    lpCaps->wXmax = 0xFFFF;
    lpCaps->wYmin = 0;
    lpCaps->wYmax = 0xFFFF;
    lpCaps->wZmin = 0;
    lpCaps->wZmax = (jstck->nrOfAxes >= 3) ? 0xFFFF : 0;
    lpCaps->wNumButtons = jstck->nrOfButtons;
    if (dwSize == sizeof(JOYCAPSW)) {
        /* complete 95 structure */
        lpCaps->wRmin = 0;
        lpCaps->wRmax = 0xFFFF;
        lpCaps->wUmin = 0;
        lpCaps->wUmax = 0xFFFF;
        lpCaps->wVmin = 0;
        lpCaps->wVmax = 0xFFFF;
        lpCaps->wMaxAxes = 6; /* same as MS Joystick Driver */
        lpCaps->wNumAxes = 0; /* nr of axes in use */
        lpCaps->wMaxButtons = 32; /* same as MS Joystick Driver */
        lpCaps->szRegKey[0] = 0;
        lpCaps->szOEMVxD[0] = 0;
        lpCaps->wCaps = 0;

        /* blank out the axes */
        for (i = 0; i < NUM_AXES; i++)
            jstck->axes[i].value_index = -1;

        for (i = 0; i < jstck->nrOfAxes; i++) {
            if (axesMap[i] < NUM_AXES)
            {
                int idx = axesMap[i];
                jstck->axes[idx].value_index = i;
                GAMEPAD_Driver->pGamePadElementProps(jstck->joyIntf, i,
                    &jstck->axes[idx].min_value, &jstck->axes[idx].max_value);
            }
            switch (axesMap[i]) {
                case DRIVER_AXIS_X:
                case DRIVER_AXIS_Y:
                    lpCaps->wNumAxes++;
                    break;
                case DRIVER_AXIS_Z:
                    lpCaps->wNumAxes++;
                    lpCaps->wCaps |= JOYCAPS_HASZ;
                    break;
                case DRIVER_AXIS_RZ:
                    lpCaps->wNumAxes++;
                    lpCaps->wCaps |= JOYCAPS_HASR;
                    break;
                case DRIVER_AXIS_RX:
                    lpCaps->wNumAxes++;
                    lpCaps->wCaps |= JOYCAPS_HASU;
                    break;
                case DRIVER_AXIS_RY:
                    lpCaps->wNumAxes++;
                    lpCaps->wCaps |= JOYCAPS_HASV;
                    break;
                default:
                    WARN("Unknown axis %i(%u). Skipped.\n", axesMap[i], i);
            }
        }
        if (jstck->nrOfPOVs > 0)
            lpCaps->wCaps |= JOYCAPS_HASPOV | JOYCAPS_POV4DIR;
    }

    return JOYERR_NOERROR;
}

/**************************************************************************
 *  driver_joyGetPos
 */
LRESULT driver_joyGetPosEx(DWORD_PTR dwDevID, LPJOYINFOEX lpInfo)
{
    static const struct {
        DWORD flag;
        off_t offset;
    } axis_map[NUM_AXES] = {
        { JOY_RETURNX, FIELD_OFFSET(JOYINFOEX, dwXpos) },
        { JOY_RETURNY, FIELD_OFFSET(JOYINFOEX, dwYpos) },
        { JOY_RETURNZ, FIELD_OFFSET(JOYINFOEX, dwZpos) },
        { JOY_RETURNU, FIELD_OFFSET(JOYINFOEX, dwUpos) },
        { JOY_RETURNV, FIELD_OFFSET(JOYINFOEX, dwVpos) },
        { JOY_RETURNR, FIELD_OFFSET(JOYINFOEX, dwRpos) },
    };

    WINE_JSTCK* jstck;
    int i;

    if ((jstck = JSTCK_drvGet(dwDevID)) == NULL)
        return MMSYSERR_NODRIVER;

    GAMEPAD_Driver->pGamePadPollValues(jstck->joyIntf, jstck->values);

    if (lpInfo->dwFlags & JOY_RETURNBUTTONS)
    {
        lpInfo->dwButtonNumber = 0;
        lpInfo->dwButtons = 0x0;
        for (i = 0; i < jstck->nrOfButtons; i++)
        {
            int data_index = jstck->nrOfAxes + (jstck->nrOfPOVs*2) + i;
            if (jstck->values[data_index]) {
                lpInfo->dwButtons |= (1 << i);
                lpInfo->dwButtonNumber++;
            }
        }
    }

    for (i = 0; i < NUM_AXES; i++)
    {
        if (lpInfo->dwFlags & axis_map[i].flag)
        {
            DWORD* field = (DWORD*)((char*)lpInfo + axis_map[i].offset);
            if (jstck->axes[i].value_index >= 0)
            {
                int dev_value = jstck->values[jstck->axes[i].value_index];
                int value = dev_value - jstck->axes[i].min_value;
                *field = MulDiv(value, 0xFFFF, jstck->axes[i].max_value - jstck->axes[i].min_value);
            }
            else
            {
                *field = 0;
                lpInfo->dwFlags &= ~axis_map[i].flag;
            }
        }
    }


    if (lpInfo->dwFlags & JOY_RETURNPOV && jstck->nrOfPOVs > 0) {
    int pov_data_index_x = jstck->nrOfAxes;
    int pov_data_index_y = jstck->nrOfAxes+1;
    if (jstck->values[pov_data_index_y]> 0) {
        if (jstck->values[pov_data_index_x]< 0)
            lpInfo->dwPOV = 22500; /* SW */
        else if (jstck->values[pov_data_index_x]> 0)
            lpInfo->dwPOV = 13500; /* SE */
        else
            lpInfo->dwPOV = 18000; /* S, JOY_POVBACKWARD */
    } else if (jstck->values[pov_data_index_y]< 0) {
        if (jstck->values[pov_data_index_x]< 0)
            lpInfo->dwPOV = 31500; /* NW */
        else if (jstck->values[pov_data_index_x]> 0)
            lpInfo->dwPOV = 4500; /* NE */
        else
            lpInfo->dwPOV = 0; /* N, JOY_POVFORWARD */
    } else if (jstck->values[pov_data_index_x]< 0)
        lpInfo->dwPOV = 27000; /* W, JOY_POVLEFT */
    else if (jstck->values[pov_data_index_x]> 0)
        lpInfo->dwPOV = 9000; /* E, JOY_POVRIGHT */
    else
        lpInfo->dwPOV = JOY_POVCENTERED; /* Center */
    }

    TRACE("x: %d, y: %d, z: %d, r: %d, u: %d, v: %d, buttons: 0x%04x, flags: 0x%04x\n",
      lpInfo->dwXpos, lpInfo->dwYpos, lpInfo->dwZpos,
      lpInfo->dwRpos, lpInfo->dwUpos, lpInfo->dwVpos,
      lpInfo->dwButtons, lpInfo->dwFlags);

    return JOYERR_NOERROR;
}

/**************************************************************************
 * driver_joyGetPos
 */
LRESULT driver_joyGetPos(DWORD_PTR dwDevID, LPJOYINFO lpInfo)
{
    JOYINFOEX   ji;
    LONG        ret;

    memset(&ji, 0, sizeof(ji));

    ji.dwSize = sizeof(ji);
    ji.dwFlags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNZ | JOY_RETURNBUTTONS;
    ret = driver_joyGetPosEx(dwDevID, &ji);
    if (ret == JOYERR_NOERROR)  {
        lpInfo->wXpos    = ji.dwXpos;
        lpInfo->wYpos    = ji.dwYpos;
        lpInfo->wZpos    = ji.dwZpos;
        lpInfo->wButtons = ji.dwButtons;
    }

    return ret;
}

static HMODULE load_graphics_driver(void)
{
    static const WCHAR display_device_guid_propW[] = {
        '_','_','w','i','n','e','_','d','i','s','p','l','a','y','_',
        'd','e','v','i','c','e','_','g','u','i','d',0 };
    static const WCHAR key_pathW[] = {
        'S','y','s','t','e','m','\\',
        'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
        'C','o','n','t','r','o','l','\\',
        'V','i','d','e','o','\\','{',0};
    static const WCHAR displayW[] = {'}','\\','0','0','0','0',0};
    static const WCHAR driverW[] = {'G','r','a','p','h','i','c','s','D','r','i','v','e','r',0};

    HMODULE ret = 0;
    HKEY hkey;
    DWORD size;
    WCHAR path[MAX_PATH];
    WCHAR key[(sizeof(key_pathW) + sizeof(displayW)) / sizeof(WCHAR) + 40];
    UINT guid_atom = HandleToULong( GetPropW( GetDesktopWindow(), display_device_guid_propW ));

    if (!guid_atom) return 0;
    memcpy( key, key_pathW, sizeof(key_pathW) );
    if (!GlobalGetAtomNameW( guid_atom, key + strlenW(key), 40 )) return 0;
    strcatW( key, displayW );
    if (RegOpenKeyW( HKEY_LOCAL_MACHINE, key, &hkey )) return 0;
    size = sizeof(path);
    if (!RegQueryValueExW( hkey, driverW, NULL, NULL, (BYTE *)path, &size )) ret = LoadLibraryW( path );
    RegCloseKey( hkey );
    TRACE( "%s %p\n", debugstr_w(path), ret );

    return ret;
}

static const GAMEPAD_DRIVER *load_driver(void)
{
    void *ptr;
    HMODULE graphics_driver;
    GAMEPAD_DRIVER *driver, *prev;

    driver = HeapAlloc( GetProcessHeap(), 0 , sizeof(*driver) );
    *driver = null_driver;

    graphics_driver = load_graphics_driver();
    if (graphics_driver)
    {
#define GET_USER_FUNC(name) \
    do { if ((ptr = GetProcAddress( graphics_driver, #name ))) driver->p##name = ptr; } while(0)
    GET_USER_FUNC(GamePadCount);
    GET_USER_FUNC(GamePadName);
    GET_USER_FUNC(GamePadElementCount);
    GET_USER_FUNC(GamePadElementProps);
    GET_USER_FUNC(GamePadPollValues);
    GET_USER_FUNC(GamePadAlloc);
    GET_USER_FUNC(GamePadDealloc);
#undef GET_USER_FUNC
    }

    prev = InterlockedCompareExchangePointer( (void **)&GAMEPAD_Driver, driver, &lazy_load_driver );
    if (prev != &lazy_load_driver)
    {
        /* another thread beat us to it */
        HeapFree( GetProcessHeap(), 0, driver);
        driver = prev;
    }
    else
        LdrAddRefDll(0, graphics_driver);

    return driver;
}

static INT CDECL loaderdrv_GamePadCount(void)
{
    return load_driver()->pGamePadCount();
}

static VOID CDECL loaderdrv_GamePadName(int id, char *name, int length)
{
    load_driver()->pGamePadName(id, name, length);
}

static VOID CDECL loaderdrv_GamePadElementCount(int id, DWORD *axis, DWORD *buttons, DWORD *povs, int axis_map[8])
{
    load_driver()->pGamePadElementCount(id, axis, buttons, povs, axis_map);
}

static VOID CDECL loaderdrv_GamePadElementProps(int id, int element, int *min, int* max)
{
    load_driver()->pGamePadElementProps(id, element, min, max);
}

static VOID CDECL loaderdrv_GamePadPollValues(int id, int* values)
{
    load_driver()->pGamePadPollValues(id, values);
}

static BOOL CDECL loaderdrv_GamePadAlloc(int id)
{
    return load_driver()->pGamePadAlloc(id);
}

static VOID CDECL loaderdrv_GamePadDealloc(int id)
{
    load_driver()->pGamePadDealloc(id);
}

static GAMEPAD_DRIVER lazy_load_driver =
{
    loaderdrv_GamePadCount,
    loaderdrv_GamePadName,
    loaderdrv_GamePadElementCount,
    loaderdrv_GamePadElementProps,
    loaderdrv_GamePadPollValues,
    loaderdrv_GamePadAlloc,
    loaderdrv_GamePadDealloc
};

static INT CDECL nulldrv_GamePadCount(void)
{
    return 0;
}

static VOID CDECL nulldrv_GamePadName(int id, char *name, int length)
{
}

static VOID CDECL nulldrv_GamePadElementCount(int id, DWORD *axis, DWORD *buttons, DWORD *povs, int axis_map[8])
{
}

static VOID CDECL nulldrv_GamePadElementProps(int id, int element, int *min, int* max)
{
}

static VOID CDECL nulldrv_GamePadPollValues(int id, int* values)
{
}

static BOOL CDECL nulldrv_GamePadAlloc(int id)
{
    return TRUE;
}

static VOID CDECL nulldrv_GamePadDealloc(int id)
{
}


static GAMEPAD_DRIVER null_driver =
{
    nulldrv_GamePadCount,
    nulldrv_GamePadName,
    nulldrv_GamePadElementCount,
    nulldrv_GamePadElementProps,
    nulldrv_GamePadPollValues,
    nulldrv_GamePadAlloc,
    nulldrv_GamePadDealloc
};

#endif /* HAVE_LINUX_JOYSTICK_H */
#endif /* HAVE_IOKIT_HID_IOHIDLIB_H */
