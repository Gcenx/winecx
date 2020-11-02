/*  DirectInput Joystick device for the Graphics Driver
 *
 * Copyright 2015 CodeWeavers, Aric Stewart
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

#include "wine/port.h"
#include <stdio.h>
#include "wine/debug.h"
#include "wine/unicode.h"
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"
#include "winternl.h"
#include "dinput.h"

#include "dinput_private.h"
#include "device_private.h"
#include "joystick_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dinput);

/* Driver interfaces */

enum { DRIVER_AXIS_X = 0,
       DRIVER_AXIS_Y,
       DRIVER_AXIS_Z,
       DRIVER_AXIS_RX,
       DRIVER_AXIS_RY,
       DRIVER_AXIS_RZ,
       DRIVER_AXIS_OTHER};

enum {
    DRIVER_CONSTANTFORCE = 0x00000001,
    DRIVER_RAMPFORCE = 0x00000002,
    DRIVER_SQUARE = 0x00000004,
    DRIVER_SINE = 0x00000008,
    DRIVER_TRIANGLE = 0x00000010,
    DRIVER_SAWTOOTHUP = 0x00000020,
    DRIVER_SAWTOOTHDOWN = 0x00000040,
    DRIVER_SPRING = 0x00000080,
    DRIVER_DAMPER = 0x00000100,
    DRIVER_INERTIA = 0x00000200,
    DRIVER_FRICTION = 0x00000400,
};

typedef struct tagGAMEPAD_DRIVER {
    INT (CDECL *pGamePadCount)(void);
    VOID (CDECL *pGamePadName)(int id, char *name, int length);
    VOID (CDECL *pGamePadElementCount)(int id, DWORD *axis, DWORD *buttons, DWORD* povs, int axis_map[8]);
    VOID (CDECL *pGamePadElementProps)(int id, int element, int *min, int *max);
    VOID (CDECL *pGamePadPollValues)(int id, int *values);

    /* Aid in driver management */
    BOOL (CDECL *pGamePadAlloc)(int id);
    VOID (CDECL *pGamePadDealloc)(int id);

    /* Force feedback interfaces */
    BOOL    (CDECL *pGamePadHasForceFeedback)(int id, DWORD *effects);
    BOOL    (CDECL *pGamePadElementHasForceFeedback)(int id, int element);
    VOID    (CDECL *pGamePadGetForceFeedbackState)(int id, DWORD *state);
    HRESULT (CDECL *pGamePadSetAutocenter)(int id, DWORD data);
    HRESULT (CDECL *pGamePadGetAutocenter)(int id, DWORD *data);
    HRESULT (CDECL *pGamePadSetFFGain)(int id, DWORD data);
    HRESULT (CDECL *pGamePadGetFFGain)(int id, DWORD *data);
    HRESULT (CDECL *pGamePadSendForceFeedbackCommand)(int id, DWORD flags);
    DWORD   (CDECL *pGamePadCreateDinputEffect)(int id, const GUID *type, const DIEFFECT *params, IDirectInputEffect **out, IUnknown *outer);
    VOID    (CDECL *pGamePadStopAllForceFeedbackEffects) (int id, int release);
} GAMEPAD_DRIVER;

static GAMEPAD_DRIVER null_driver, lazy_load_driver;

const GAMEPAD_DRIVER *GAMEPAD_Driver = &lazy_load_driver;

typedef struct JoystickImpl JoystickImpl;
static const IDirectInputDevice8AVtbl JoystickAvt;
static const IDirectInputDevice8WVtbl JoystickWvt;

/* Driver interfaces */
static void polldev(LPDIRECTINPUTDEVICE8A iface);

struct JoystickImpl
{
    struct JoystickGenericImpl generic;

    /* private */
    int id;
    int axis_map[8];
    int *values;
};

static inline JoystickImpl *impl_from_IDirectInputDevice8A(IDirectInputDevice8A *iface)
{
    return CONTAINING_RECORD(CONTAINING_RECORD(CONTAINING_RECORD(iface, IDirectInputDeviceImpl, IDirectInputDevice8A_iface),
           JoystickGenericImpl, base), JoystickImpl, generic);
}
static inline JoystickImpl *impl_from_IDirectInputDevice8W(IDirectInputDevice8W *iface)
{
    return CONTAINING_RECORD(CONTAINING_RECORD(CONTAINING_RECORD(iface, IDirectInputDeviceImpl, IDirectInputDevice8W_iface),
           JoystickGenericImpl, base), JoystickImpl, generic);
}

static const GUID DInput_Wine_driver_Joystick_GUID = { /* B7C30F45-1F2C-4C79-8FD3-12A4E10C7646*/
  0xB7C30F45, 0x1F2C, 0x4C79, {0x8F, 0xD3, 0x12, 0xA4, 0xE1, 0x0C, 0x76, 0x46}
};

static INT find_joystick_devices(void)
{
    static INT joystick_devices_count = -1;

    if (joystick_devices_count != -1) return joystick_devices_count;

    joystick_devices_count = GAMEPAD_Driver->pGamePadCount();

    return  joystick_devices_count;
}

static HRESULT joydev_enum_deviceA(DWORD dwDevType, DWORD dwFlags, LPDIDEVICEINSTANCEA lpddi, DWORD version, int id)
{
    TRACE("dwDevType %u dwFlags 0x%08x version 0x%04x id %d\n", dwDevType, dwFlags, version, id);

    if (id >= find_joystick_devices()) return E_FAIL;

    if ((dwDevType == 0) ||
    ((dwDevType == DIDEVTYPE_JOYSTICK) && (version > 0x0300 && version < 0x0800)) ||
    (((dwDevType == DI8DEVCLASS_GAMECTRL) || (dwDevType == DI8DEVTYPE_JOYSTICK)) && (version >= 0x0800)))
    {
        if (dwFlags & DIEDFL_FORCEFEEDBACK) {
            if (!GAMEPAD_Driver->pGamePadHasForceFeedback(id, NULL))
                return S_FALSE;
        }
        /* Return joystick */
        lpddi->guidInstance = DInput_Wine_driver_Joystick_GUID;
        lpddi->guidInstance.Data3 = id;
        lpddi->guidProduct = DInput_Wine_driver_Joystick_GUID;
        /* we only support traditional joysticks for now */
        if (version >= 0x0800)
            lpddi->dwDevType = DI8DEVTYPE_JOYSTICK | (DI8DEVTYPEJOYSTICK_STANDARD << 8);
        else
            lpddi->dwDevType = DIDEVTYPE_JOYSTICK | (DIDEVTYPEJOYSTICK_TRADITIONAL << 8);
        sprintf(lpddi->tszInstanceName, "Joystick %d", id);

        /* get the device name */
        GAMEPAD_Driver->pGamePadName(id, lpddi->tszProductName, MAX_PATH);

        lpddi->guidFFDriver = GUID_NULL;
        return S_OK;
    }

    return S_FALSE;
}

static HRESULT joydev_enum_deviceW(DWORD dwDevType, DWORD dwFlags, LPDIDEVICEINSTANCEW lpddi, DWORD version, int id)
{
    char name[MAX_PATH];
    char friendly[32];

    TRACE("dwDevType %u dwFlags 0x%08x version 0x%04x id %d\n", dwDevType, dwFlags, version, id);

    if (id >= find_joystick_devices()) return E_FAIL;
    if ((dwDevType == 0) ||
    ((dwDevType == DIDEVTYPE_JOYSTICK) && (version > 0x0300 && version < 0x0800)) ||
    (((dwDevType == DI8DEVCLASS_GAMECTRL) || (dwDevType == DI8DEVTYPE_JOYSTICK)) && (version >= 0x0800)))
    {
        if (dwFlags & DIEDFL_FORCEFEEDBACK) {
            if (!GAMEPAD_Driver->pGamePadHasForceFeedback(id, NULL))
                return S_FALSE;
        }
        /* Return joystick */
        lpddi->guidInstance = DInput_Wine_driver_Joystick_GUID;
        lpddi->guidInstance.Data3 = id;
        lpddi->guidProduct = DInput_Wine_driver_Joystick_GUID;
        /* we only support traditional joysticks for now */
        if (version >= 0x0800)
            lpddi->dwDevType = DI8DEVTYPE_JOYSTICK | (DI8DEVTYPEJOYSTICK_STANDARD << 8);
        else
            lpddi->dwDevType = DIDEVTYPE_JOYSTICK | (DIDEVTYPEJOYSTICK_TRADITIONAL << 8);
        sprintf(friendly, "Joystick %d", id);
        MultiByteToWideChar(CP_ACP, 0, friendly, -1, lpddi->tszInstanceName, MAX_PATH);
        /* get the device name */
        GAMEPAD_Driver->pGamePadName(id, name, MAX_PATH);

        MultiByteToWideChar(CP_ACP, 0, name, -1, lpddi->tszProductName, MAX_PATH);
        lpddi->guidFFDriver = GUID_NULL;
        return S_OK;
    }

    return S_FALSE;
}

static HRESULT alloc_device(REFGUID rguid, IDirectInputImpl *dinput,
                            JoystickImpl **pdev, unsigned short index)
{
    DWORD i;
    JoystickImpl* newDevice;
    char name[MAX_PATH];
    HRESULT hr;
    LPDIDATAFORMAT df = NULL;
    int idx = 0;
    int slider_count = 0;

    TRACE("%s %p %p %hu\n", debugstr_guid(rguid), dinput, pdev, index);

    if (!GAMEPAD_Driver->pGamePadAlloc(index))
    {
        WARN("Driver gamepad alloc failed\n");
        *pdev = 0;
        return DIERR_OUTOFMEMORY;
    }

    newDevice = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(JoystickImpl));
    if (newDevice == 0) {
        WARN("out of memory\n");
        *pdev = 0;
        return DIERR_OUTOFMEMORY;
    }

    newDevice->id = index;

    newDevice->generic.guidInstance = DInput_Wine_driver_Joystick_GUID;
    newDevice->generic.guidInstance.Data3 = index;
    newDevice->generic.guidProduct = DInput_Wine_driver_Joystick_GUID;
    newDevice->generic.joy_polldev = polldev;

    /* get the device name */
    GAMEPAD_Driver->pGamePadName(index, name, MAX_PATH);
    TRACE("Name %s\n",name);

    /* copy the device name */
    newDevice->generic.name = HeapAlloc(GetProcessHeap(),0,strlen(name) + 1);
    strcpy(newDevice->generic.name, name);

    GAMEPAD_Driver->pGamePadElementCount(index,
        &newDevice->generic.devcaps.dwAxes,
        &newDevice->generic.devcaps.dwButtons,
        &newDevice->generic.devcaps.dwPOVs, newDevice->axis_map);

    if (GAMEPAD_Driver->pGamePadHasForceFeedback(index, NULL))
        newDevice->generic.devcaps.dwFlags |= DIDC_FORCEFEEDBACK;

    TRACE("%i axes %i buttons %i povs\n",newDevice->generic.devcaps.dwAxes,newDevice->generic.devcaps.dwButtons,newDevice->generic.devcaps.dwPOVs);

    if (newDevice->generic.devcaps.dwButtons > 128)
    {
        WARN("Can't support %d buttons. Clamping down to 128\n", newDevice->generic.devcaps.dwButtons);
        newDevice->generic.devcaps.dwButtons = 128;
    }

    newDevice->generic.base.IDirectInputDevice8A_iface.lpVtbl = &JoystickAvt;
    newDevice->generic.base.IDirectInputDevice8W_iface.lpVtbl = &JoystickWvt;
    newDevice->generic.base.ref = 1;
    newDevice->generic.base.dinput = dinput;
    newDevice->generic.base.guid = *rguid;
    InitializeCriticalSection(&newDevice->generic.base.crit);
    newDevice->generic.base.crit.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": JoystickImpl*->generic.base.crit");

    /* Create copy of default data format */
    if (!(df = HeapAlloc(GetProcessHeap(), 0, c_dfDIJoystick2.dwSize))) goto FAILED;
    memcpy(df, &c_dfDIJoystick2, c_dfDIJoystick2.dwSize);

    df->dwNumObjs = newDevice->generic.devcaps.dwAxes + newDevice->generic.devcaps.dwPOVs + newDevice->generic.devcaps.dwButtons;
    if (!(df->rgodf = HeapAlloc(GetProcessHeap(), 0, df->dwNumObjs * df->dwObjSize))) goto FAILED;

    newDevice->values = HeapAlloc(GetProcessHeap(), 0, sizeof(int) * df->dwNumObjs);

    for (i = 0; i < newDevice->generic.devcaps.dwAxes; i++)
    {
        int wine_obj = newDevice->axis_map[i];
        if (wine_obj == DRIVER_AXIS_OTHER)
        {
            wine_obj = DRIVER_AXIS_OTHER + slider_count;
            slider_count++;
        }
        if (wine_obj < 0 ) continue;

        memcpy(&df->rgodf[idx], &c_dfDIJoystick2.rgodf[wine_obj], df->dwObjSize);
        df->rgodf[idx].dwType = DIDFT_MAKEINSTANCE(wine_obj) | DIDFT_ABSAXIS;
        if (GAMEPAD_Driver->pGamePadElementHasForceFeedback(index, idx))
            df->rgodf[idx].dwFlags |= DIDOI_FFACTUATOR;
        ++idx;
    }

    for (i = 0; i < newDevice->generic.devcaps.dwPOVs; i++)
    {
        memcpy(&df->rgodf[idx], &c_dfDIJoystick2.rgodf[i + 8], df->dwObjSize);
        df->rgodf[idx++].dwType = DIDFT_MAKEINSTANCE(i) | DIDFT_POV;
    }

    for (i = 0; i < newDevice->generic.devcaps.dwButtons; i++)
    {
        memcpy(&df->rgodf[idx], &c_dfDIJoystick2.rgodf[i + 12], df->dwObjSize);
        df->rgodf[idx  ].pguid = &GUID_Button;
        df->rgodf[idx++].dwType = DIDFT_MAKEINSTANCE(i) | DIDFT_PSHBUTTON;
    }
    newDevice->generic.base.data_format.wine_df = df;

    /* initialize properties */
    for (i = 0; i < df->dwNumObjs; i++)
    {
        int min, max;
        GAMEPAD_Driver->pGamePadElementProps(index, i, &min, &max);
        newDevice->generic.props[i].lDevMin = min;
        newDevice->generic.props[i].lDevMax = max;
        newDevice->generic.props[i].lMin =  0;
        newDevice->generic.props[i].lMax =  0xffff;
        newDevice->generic.props[i].lDeadZone = 0;
        newDevice->generic.props[i].lSaturation = 0;
    }

    IDirectInput_AddRef(&newDevice->generic.base.dinput->IDirectInput7A_iface);

    EnterCriticalSection(&dinput->crit);
    list_add_tail(&dinput->devices_list, &newDevice->generic.base.entry);
    LeaveCriticalSection(&dinput->crit);

    newDevice->generic.devcaps.dwSize = sizeof(newDevice->generic.devcaps);
    newDevice->generic.devcaps.dwFlags |= DIDC_ATTACHED;
    if (newDevice->generic.base.dinput->dwVersion >= 0x0800)
        newDevice->generic.devcaps.dwDevType = DI8DEVTYPE_JOYSTICK | (DI8DEVTYPEJOYSTICK_STANDARD << 8);
    else
        newDevice->generic.devcaps.dwDevType = DIDEVTYPE_JOYSTICK | (DIDEVTYPEJOYSTICK_TRADITIONAL << 8);
    newDevice->generic.devcaps.dwFFSamplePeriod = 0;
    newDevice->generic.devcaps.dwFFMinTimeResolution = 0;
    newDevice->generic.devcaps.dwFirmwareRevision = 0;
    newDevice->generic.devcaps.dwHardwareRevision = 0;
    newDevice->generic.devcaps.dwFFDriverVersion = 0;

    if (TRACE_ON(dinput)) {
        TRACE("allocated device %p\n", newDevice);
        _dump_DIDATAFORMAT(newDevice->generic.base.data_format.wine_df);
        _dump_DIDEVCAPS(&newDevice->generic.devcaps);
    }

    *pdev = newDevice;

    return DI_OK;

FAILED:
    hr = DIERR_OUTOFMEMORY;
    if (df) HeapFree(GetProcessHeap(), 0, df->rgodf);
    HeapFree(GetProcessHeap(), 0, df);
    release_DataFormat(&newDevice->generic.base.data_format);
    HeapFree(GetProcessHeap(),0,newDevice->generic.name);
    HeapFree(GetProcessHeap(),0,newDevice);
    *pdev = 0;

    return hr;
}

/******************************************************************************
  *     get_joystick_index : Get the joystick index from a given GUID
  */
static unsigned short get_joystick_index(REFGUID guid)
{
    GUID wine_joystick = DInput_Wine_driver_Joystick_GUID;
    GUID dev_guid = *guid;

    wine_joystick.Data3 = 0;
    dev_guid.Data3 = 0;

    /* for the standard joystick GUID use index 0 */
    if(IsEqualGUID(&GUID_Joystick,guid)) return 0;

    /* for the wine joystick GUIDs use the index stored in Data3 */
    if(IsEqualGUID(&wine_joystick, &dev_guid)) return guid->Data3;

    return 0xffff;
}

static HRESULT joydev_create_device(IDirectInputImpl *dinput, REFGUID rguid, REFIID riid, LPVOID *pdev, int unicode)
{
    unsigned short index;
    int joystick_devices_count;

    TRACE("%p %s %s %p %i\n", dinput, debugstr_guid(rguid), debugstr_guid(riid), pdev, unicode);
    *pdev = NULL;

    if ((joystick_devices_count = find_joystick_devices()) == 0)
        return DIERR_DEVICENOTREG;

    if ((index = get_joystick_index(rguid)) < 0xffff &&
        joystick_devices_count && index < joystick_devices_count)
    {
        JoystickImpl *This;
        HRESULT hr;

        if (riid == NULL)
            ;/* nothing */
        else if (IsEqualGUID(&IID_IDirectInputDeviceA,  riid) ||
                 IsEqualGUID(&IID_IDirectInputDevice2A, riid) ||
                 IsEqualGUID(&IID_IDirectInputDevice7A, riid) ||
                 IsEqualGUID(&IID_IDirectInputDevice8A, riid))
        {
            unicode = 0;
        }
        else if (IsEqualGUID(&IID_IDirectInputDeviceW,  riid) ||
                 IsEqualGUID(&IID_IDirectInputDevice2W, riid) ||
                 IsEqualGUID(&IID_IDirectInputDevice7W, riid) ||
                 IsEqualGUID(&IID_IDirectInputDevice8W, riid))
        {
            unicode = 1;
        }
        else
        {
            WARN("no interface\n");
            return DIERR_NOINTERFACE;
        }

        hr = alloc_device(rguid, dinput, &This, index);
        if (!This) return hr;

        if (unicode)
            *pdev = &This->generic.base.IDirectInputDevice8W_iface;
        else
            *pdev = &This->generic.base.IDirectInputDevice8A_iface;
        return hr;
    }

    return DIERR_DEVICENOTREG;
}

static ULONG WINAPI JoystickAImpl_Release(LPDIRECTINPUTDEVICE8A iface)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);
    INT id = This->id;
    ULONG ref = IDirectInputDevice2AImpl_Release(iface);

    if (!ref)
        GAMEPAD_Driver->pGamePadDealloc(id);

    return ref;
}

static ULONG WINAPI JoystickWImpl_Release(LPDIRECTINPUTDEVICE8W iface)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8W(iface);
    INT id = This->id;
    ULONG ref = IDirectInputDevice2WImpl_Release(iface);

    if (!ref)
        GAMEPAD_Driver->pGamePadDealloc(id);

    return ref;
}

static HRESULT WINAPI JoystickWImpl_GetProperty(LPDIRECTINPUTDEVICE8W iface, REFGUID rguid, LPDIPROPHEADER pdiph)
{
    HRESULT hr = DI_OK;
    JoystickImpl *This = impl_from_IDirectInputDevice8W(iface);

    TRACE("(this=%p,%s,%p)\n", iface, debugstr_guid(rguid), pdiph);
    _dump_DIPROPHEADER(pdiph);

    if (!IS_DIPROP(rguid)) return DI_OK;

    switch (LOWORD(rguid)) {

       case (DWORD_PTR) DIPROP_JOYSTICKID:
        {
            LPDIPROPDWORD pd = (LPDIPROPDWORD)pdiph;

            pd->dwData = This->id;
            TRACE("DIPROP_JOYSTICKID(%d)\n", pd->dwData);
            break;
        }

        case (DWORD_PTR)DIPROP_AUTOCENTER:
        {
            LPDIPROPDWORD pd = (LPDIPROPDWORD)pdiph;
            hr = GAMEPAD_Driver->pGamePadGetAutocenter(This->id, &pd->dwData);
            break;
        }

        case (DWORD_PTR)DIPROP_FFGAIN:
        {
            LPDIPROPDWORD pd = (LPDIPROPDWORD)pdiph;
            hr = GAMEPAD_Driver->pGamePadGetFFGain(This->id, &pd->dwData);
            break;
        }

    default:
        return JoystickWGenericImpl_GetProperty(iface, rguid, pdiph);
    }

    return hr;
}

static HRESULT WINAPI JoystickWImpl_SetProperty(IDirectInputDevice8W *iface,
        const GUID *prop, const DIPROPHEADER *header)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8W(iface);

    TRACE("%p %s %p\n", This, debugstr_guid(prop), header);
    switch(LOWORD(prop))
    {
    case (DWORD_PTR)DIPROP_AUTOCENTER:
        return GAMEPAD_Driver->pGamePadSetAutocenter(This->id,
            ((const DIPROPDWORD *)header)->dwData);
    case (DWORD_PTR)DIPROP_FFGAIN:
        return GAMEPAD_Driver->pGamePadSetFFGain(This->id,
            ((const DIPROPDWORD *)header)->dwData);
    }

    return JoystickWGenericImpl_SetProperty(iface, prop, header);
}

static HRESULT WINAPI JoystickAImpl_GetProperty(LPDIRECTINPUTDEVICE8A iface, REFGUID rguid, LPDIPROPHEADER pdiph)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);
    return JoystickWImpl_GetProperty(&This->generic.base.IDirectInputDevice8W_iface, rguid, pdiph);
}


static HRESULT WINAPI JoystickAImpl_SetProperty(IDirectInputDevice8A *iface,
        const GUID *prop, const DIPROPHEADER *header)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);

    TRACE("%p %s %p\n", This, debugstr_guid(prop), header);

    switch(LOWORD(prop))
    {
    case (DWORD_PTR)DIPROP_AUTOCENTER:
        return GAMEPAD_Driver->pGamePadSetAutocenter(This->id,
            ((const DIPROPDWORD *)header)->dwData);
    case (DWORD_PTR)DIPROP_FFGAIN:
        return GAMEPAD_Driver->pGamePadSetFFGain(This->id,
            ((const DIPROPDWORD *)header)->dwData);
    }

    return JoystickAGenericImpl_SetProperty(iface, prop, header);
}

static HRESULT WINAPI JoystickWImpl_Unacquire(LPDIRECTINPUTDEVICE8W iface)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8W(iface);
    HRESULT res;

    TRACE("(this=%p)\n",This);
    res = IDirectInputDevice2WImpl_Unacquire(iface);
    if (res==DI_OK) {
        GAMEPAD_Driver->pGamePadStopAllForceFeedbackEffects(This->id, FALSE);
        /* Enable autocenter. */
        GAMEPAD_Driver->pGamePadSetAutocenter(This->id, DIPROPAUTOCENTER_ON);
    }
    return res;
}

static HRESULT WINAPI JoystickAImpl_Unacquire(LPDIRECTINPUTDEVICE8A iface)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);
    return JoystickWImpl_Unacquire(&This->generic.base.IDirectInputDevice8W_iface);
}


static HRESULT WINAPI JoystickWImpl_CreateEffect(IDirectInputDevice8W *iface,
        const GUID *type, const DIEFFECT *params, IDirectInputEffect **out,
        IUnknown *outer)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8W(iface);
    TRACE("%p %s %p %p %p\n", iface, debugstr_guid(type), params, out, outer);
    dump_DIEFFECT(params, type, 0);

    return GAMEPAD_Driver->pGamePadCreateDinputEffect(This->id, type, params,
        out, outer);
}

static HRESULT WINAPI JoystickAImpl_CreateEffect(IDirectInputDevice8A *iface,
        const GUID *type, const DIEFFECT *params, IDirectInputEffect **out,
        IUnknown *outer)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);

    TRACE("%p %s %p %p %p\n", iface, debugstr_guid(type), params, out, outer);

    return JoystickWImpl_CreateEffect(&This->generic.base.IDirectInputDevice8W_iface,
            type, params, out, outer);
}

static HRESULT WINAPI JoystickWImpl_SendForceFeedbackCommand(IDirectInputDevice8W *iface,
        DWORD flags)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8W(iface);

    TRACE("%p 0x%x\n", This, flags);
    return GAMEPAD_Driver->pGamePadSendForceFeedbackCommand(This->id, flags);
}

static HRESULT WINAPI JoystickAImpl_SendForceFeedbackCommand(IDirectInputDevice8A *iface,
        DWORD flags)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);

    TRACE("%p 0x%x\n", This, flags);

    return JoystickWImpl_SendForceFeedbackCommand(&This->generic.base.IDirectInputDevice8W_iface, flags);
}

static HRESULT WINAPI JoystickAImpl_EnumEffects(LPDIRECTINPUTDEVICE8A iface,
                                                LPDIENUMEFFECTSCALLBACKA lpCallback,
                                                LPVOID pvRef,
                                                DWORD dwEffType)
{
    DIEFFECTINFOA dei; /* feif */
    DWORD type = DIEFT_GETTYPE(dwEffType);
    DWORD effects;
    JoystickImpl* This = impl_from_IDirectInputDevice8A(iface);

    TRACE("(this=%p,%p,%d) type=%d\n", This, pvRef, dwEffType, type);

    if (GAMEPAD_Driver->pGamePadHasForceFeedback(This->id, &effects))
    {
        dei.dwSize = sizeof(DIEFFECTINFOA);

        if ((type == DIEFT_ALL || type == DIEFT_CONSTANTFORCE) &&
            effects & DRIVER_CONSTANTFORCE)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_ConstantForce);
            (*lpCallback)(&dei, pvRef);
        }

        if ((type == DIEFT_ALL || type == DIEFT_PERIODIC))
        {
            if (effects & DRIVER_SQUARE)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Square);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_SINE)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Sine);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_TRIANGLE)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Triangle);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_SAWTOOTHUP)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_SawtoothUp);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_SAWTOOTHDOWN)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_SawtoothDown);
                (*lpCallback)(&dei, pvRef);
            }
        }

        if ((type == DIEFT_ALL || type == DIEFT_RAMPFORCE) &&
            effects & DRIVER_RAMPFORCE)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_RampForce);
            (*lpCallback)(&dei, pvRef);
        }

        if (type == DIEFT_ALL || type == DIEFT_CONDITION) {
            if (effects & DRIVER_SPRING)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Spring);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_DAMPER)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Damper);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_INERTIA)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Inertia);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_FRICTION)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Friction);
                (*lpCallback)(&dei, pvRef);
            }
        }
    }
    return DI_OK;
}

static HRESULT WINAPI JoystickWImpl_EnumEffects(LPDIRECTINPUTDEVICE8W iface,
                                                LPDIENUMEFFECTSCALLBACKW lpCallback,
                                                LPVOID pvRef,
                                                DWORD dwEffType)
{
    /* seems silly to duplicate all this code but all the structures and functions
     * are actually different (A/W) */
    DIEFFECTINFOW dei; /* feif */
    DWORD type = DIEFT_GETTYPE(dwEffType);
    DWORD effects;
    JoystickImpl* This = impl_from_IDirectInputDevice8W(iface);

    TRACE("(this=%p,%p,%d) type=%d\n", This, pvRef, dwEffType, type);

    if (GAMEPAD_Driver->pGamePadHasForceFeedback(This->id, &effects))
    {
        dei.dwSize = sizeof(DIEFFECTINFOW);

        if ((type == DIEFT_ALL || type == DIEFT_CONSTANTFORCE) &&
            effects & DRIVER_CONSTANTFORCE)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_ConstantForce);
            (*lpCallback)(&dei, pvRef);
        }

        if ((type == DIEFT_ALL || type == DIEFT_PERIODIC))
        {
            if (effects & DRIVER_SQUARE)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Square);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_SINE)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Sine);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_TRIANGLE)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Triangle);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_SAWTOOTHUP)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_SawtoothUp);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_SAWTOOTHDOWN)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_SawtoothDown);
                (*lpCallback)(&dei, pvRef);
            }
        }

        if ((type == DIEFT_ALL || type == DIEFT_RAMPFORCE) &&
            effects & DRIVER_RAMPFORCE)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_RampForce);
            (*lpCallback)(&dei, pvRef);
        }

        if (type == DIEFT_ALL || type == DIEFT_CONDITION) {
            if (effects & DRIVER_SPRING)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Spring);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_DAMPER)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Damper);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_INERTIA)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Inertia);
                (*lpCallback)(&dei, pvRef);
            }
            if (effects & DRIVER_FRICTION)
            {
                IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Friction);
                (*lpCallback)(&dei, pvRef);
            }
        }
    }
    return DI_OK;
}

static HRESULT WINAPI JoystickAImpl_GetEffectInfo(LPDIRECTINPUTDEVICE8A iface,
                                                  LPDIEFFECTINFOA pdei,
                                                  REFGUID guid)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);

    TRACE("(this=%p,%p,%s)\n", This, pdei, _dump_dinput_GUID(guid));

    if (GAMEPAD_Driver->pGamePadHasForceFeedback(This->id, NULL))
    {
        DWORD type = typeFromGUID(guid);

        TRACE("(%s, %p) type=%d\n", _dump_dinput_GUID(guid), pdei, type);

        if (!pdei) return E_POINTER;

        if (pdei->dwSize != sizeof(DIEFFECTINFOA)) return DIERR_INVALIDPARAM;

        pdei->guid = *guid;

        pdei->dwEffType = type;
        /* the event device API does not support querying for all these things
         * therefore we assume that we have support for them
         * that's not as dangerous as it sounds, since drivers are allowed to
         * ignore parameters they claim to support anyway */
        pdei->dwEffType |= DIEFT_DEADBAND | DIEFT_FFATTACK | DIEFT_FFFADE
                        | DIEFT_POSNEGCOEFFICIENTS | DIEFT_POSNEGSATURATION
                        | DIEFT_SATURATION | DIEFT_STARTDELAY;

        /* again, assume we have support for everything */
        pdei->dwStaticParams = DIEP_ALLPARAMS;
        pdei->dwDynamicParams = pdei->dwStaticParams;

        /* yes, this is windows behavior (print the GUID_Name for name) */
        strcpy(pdei->tszName, _dump_dinput_GUID(guid));
    }

    return DI_OK;
}


static HRESULT WINAPI JoystickWImpl_GetEffectInfo(LPDIRECTINPUTDEVICE8W iface,
                                                  LPDIEFFECTINFOW pdei,
                                                  REFGUID guid)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8W(iface);

    TRACE("(this=%p,%p,%s)\n", This, pdei, _dump_dinput_GUID(guid));

    if (GAMEPAD_Driver->pGamePadHasForceFeedback(This->id, NULL))
    {
        DWORD type = typeFromGUID(guid);

        TRACE("(%s, %p) type=%d\n", _dump_dinput_GUID(guid), pdei, type);

        if (!pdei) return E_POINTER;

        if (pdei->dwSize != sizeof(DIEFFECTINFOW)) return DIERR_INVALIDPARAM;

        pdei->guid = *guid;

        pdei->dwEffType = type;
        /* the event device API does not support querying for all these things
         * therefore we assume that we have support for them
         * that's not as dangerous as it sounds, since drivers are allowed to
         * ignore parameters they claim to support anyway */
        pdei->dwEffType |= DIEFT_DEADBAND | DIEFT_FFATTACK | DIEFT_FFFADE
                        | DIEFT_POSNEGCOEFFICIENTS | DIEFT_POSNEGSATURATION
                        | DIEFT_SATURATION | DIEFT_STARTDELAY;

        /* again, assume we have support for everything */
        pdei->dwStaticParams = DIEP_ALLPARAMS;
        pdei->dwDynamicParams = pdei->dwStaticParams;

        /* yes, this is windows behavior (print the GUID_Name for name) */
        MultiByteToWideChar(CP_ACP, 0, _dump_dinput_GUID(guid), -1,
                            pdei->tszName, MAX_PATH);
    }

    return DI_OK;
}

static HRESULT WINAPI JoystickWImpl_GetForceFeedbackState(LPDIRECTINPUTDEVICE8W iface, LPDWORD pdwOut)
{
    JoystickImpl* This = impl_from_IDirectInputDevice8W(iface);

    TRACE("(this=%p,%p)\n", This, pdwOut);

    (*pdwOut) = 0;
    GAMEPAD_Driver->pGamePadGetForceFeedbackState(This->id, pdwOut);

    return DI_OK;
}

static HRESULT WINAPI JoystickAImpl_GetForceFeedbackState(LPDIRECTINPUTDEVICE8A iface, LPDWORD pdwOut)
{
    JoystickImpl* This = impl_from_IDirectInputDevice8A(iface);

    TRACE("(this=%p,%p)\n", This, pdwOut);

    (*pdwOut) = 0;
    GAMEPAD_Driver->pGamePadGetForceFeedbackState(This->id, pdwOut);

    return DI_OK;
}

const struct dinput_device joystick_driver_device = {
  "Wine Graphic Driver joystick driver",
  joydev_enum_deviceA,
  joydev_enum_deviceW,
  joydev_create_device
};

static const IDirectInputDevice8AVtbl JoystickAvt =
{
    IDirectInputDevice2AImpl_QueryInterface,
    IDirectInputDevice2AImpl_AddRef,
    JoystickAImpl_Release,
    JoystickAGenericImpl_GetCapabilities,
    IDirectInputDevice2AImpl_EnumObjects,
    JoystickAImpl_GetProperty,
    JoystickAImpl_SetProperty,
    IDirectInputDevice2AImpl_Acquire,
    JoystickAImpl_Unacquire,
    JoystickAGenericImpl_GetDeviceState,
    IDirectInputDevice2AImpl_GetDeviceData,
    IDirectInputDevice2AImpl_SetDataFormat,
    IDirectInputDevice2AImpl_SetEventNotification,
    IDirectInputDevice2AImpl_SetCooperativeLevel,
    JoystickAGenericImpl_GetObjectInfo,
    JoystickAGenericImpl_GetDeviceInfo,
    IDirectInputDevice2AImpl_RunControlPanel,
    IDirectInputDevice2AImpl_Initialize,
    JoystickAImpl_CreateEffect,
    JoystickAImpl_EnumEffects,
    JoystickAImpl_GetEffectInfo,
    JoystickAImpl_GetForceFeedbackState,
    JoystickAImpl_SendForceFeedbackCommand,
    IDirectInputDevice2AImpl_EnumCreatedEffectObjects,
    IDirectInputDevice2AImpl_Escape,
    JoystickAGenericImpl_Poll,
    IDirectInputDevice2AImpl_SendDeviceData,
    IDirectInputDevice7AImpl_EnumEffectsInFile,
    IDirectInputDevice7AImpl_WriteEffectToFile,
    JoystickAGenericImpl_BuildActionMap,
    JoystickAGenericImpl_SetActionMap,
    IDirectInputDevice8AImpl_GetImageInfo
};

static const IDirectInputDevice8WVtbl JoystickWvt =
{
    IDirectInputDevice2WImpl_QueryInterface,
    IDirectInputDevice2WImpl_AddRef,
    JoystickWImpl_Release,
    JoystickWGenericImpl_GetCapabilities,
    IDirectInputDevice2WImpl_EnumObjects,
    JoystickWImpl_GetProperty,
    JoystickWImpl_SetProperty,
    IDirectInputDevice2WImpl_Acquire,
    JoystickWImpl_Unacquire,
    JoystickWGenericImpl_GetDeviceState,
    IDirectInputDevice2WImpl_GetDeviceData,
    IDirectInputDevice2WImpl_SetDataFormat,
    IDirectInputDevice2WImpl_SetEventNotification,
    IDirectInputDevice2WImpl_SetCooperativeLevel,
    JoystickWGenericImpl_GetObjectInfo,
    JoystickWGenericImpl_GetDeviceInfo,
    IDirectInputDevice2WImpl_RunControlPanel,
    IDirectInputDevice2WImpl_Initialize,
    JoystickWImpl_CreateEffect,
    JoystickWImpl_EnumEffects,
    JoystickWImpl_GetEffectInfo,
    JoystickWImpl_GetForceFeedbackState,
    JoystickWImpl_SendForceFeedbackCommand,
    IDirectInputDevice2WImpl_EnumCreatedEffectObjects,
    IDirectInputDevice2WImpl_Escape,
    JoystickWGenericImpl_Poll,
    IDirectInputDevice2WImpl_SendDeviceData,
    IDirectInputDevice7WImpl_EnumEffectsInFile,
    IDirectInputDevice7WImpl_WriteEffectToFile,
    JoystickWGenericImpl_BuildActionMap,
    JoystickWGenericImpl_SetActionMap,
    IDirectInputDevice8WImpl_GetImageInfo
};

static void polldev(LPDIRECTINPUTDEVICE8A iface)
{
    int inst_id, i, driver_index = 0;
    int  oldVal = 0, newVal = 0;
    int slider_idx = 0;
    int pov_idx = 0;
    int button_idx = 0;
    JoystickImpl *device = impl_from_IDirectInputDevice8A(iface);

    TRACE("device %p device->id %i\n", device, device->id);
    GAMEPAD_Driver->pGamePadPollValues(device->id, device->values);

    for (i = 0; i < device->generic.devcaps.dwAxes; i++)
    {
        int wine_obj = device->axis_map[i];
        newVal = joystick_map_axis(&device->generic.props[i],
                    device->values[driver_index++]);
        switch (device->axis_map[i])
        {
            case DRIVER_AXIS_X:
                oldVal = device->generic.js.lX;
                device->generic.js.lX = newVal;
                break;
            case DRIVER_AXIS_Y:
                oldVal = device->generic.js.lY;
                device->generic.js.lY = newVal;
                break;
            case DRIVER_AXIS_Z:
                oldVal = device->generic.js.lZ;
                device->generic.js.lZ = newVal;
                break;
            case DRIVER_AXIS_RX:
                oldVal = device->generic.js.lRx;
                device->generic.js.lRx = newVal;
                break;
            case DRIVER_AXIS_RY:
                oldVal = device->generic.js.lRy;
                device->generic.js.lRy = newVal;
                break;
            case DRIVER_AXIS_RZ:
                oldVal = device->generic.js.lRz;
                device->generic.js.lRz = newVal;
                break;
            case DRIVER_AXIS_OTHER:
                wine_obj += slider_idx;
                oldVal = device->generic.js.rglSlider[slider_idx];
                device->generic.js.rglSlider[slider_idx] = newVal;
                slider_idx ++;
        }
        TRACE("oldVal %d newVal %d\n", oldVal, newVal);
        if ((wine_obj != -1) && (oldVal != newVal))
        {
            inst_id = DIDFT_MAKEINSTANCE(wine_obj) | DIDFT_ABSAXIS;
            queue_event(iface,inst_id,newVal,GetCurrentTime(),device->generic.base.dinput->evsequence++);
        }
    }
    for (i = 0; i < device->generic.devcaps.dwPOVs; i++)
    {
        POINTL  pov_val;
        pov_val.x = device->values[driver_index++];
        pov_val.y = device->values[driver_index++];
        newVal = joystick_map_pov(&pov_val);
        oldVal = device->generic.js.rgdwPOV[pov_idx];
        device->generic.js.rgdwPOV[pov_idx] = newVal;
        TRACE("oldVal %d newVal %d\n", oldVal, newVal);
        if (oldVal != newVal)
        {
            inst_id = DIDFT_MAKEINSTANCE(pov_idx) | DIDFT_POV;
            queue_event(iface,inst_id,newVal,GetCurrentTime(),device->generic.base.dinput->evsequence++);
        }
        pov_idx ++;
        break;
    }
    for (i = 0; i < device->generic.devcaps.dwButtons; i++)
    {
        newVal = device->values[driver_index++] ? 0x80 : 0x0;
        oldVal = device->generic.js.rgbButtons[button_idx];
        device->generic.js.rgbButtons[button_idx] = newVal;
        TRACE("oldVal %d newVal %d\n", oldVal, newVal);
        if (oldVal != newVal)
        {
            inst_id = DIDFT_MAKEINSTANCE(0) | DIDFT_PSHBUTTON;
            queue_event(iface,inst_id,newVal,GetCurrentTime(),device->generic.base.dinput->evsequence++);
        }
        button_idx ++;
    }

    return;
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
    GET_USER_FUNC(GamePadHasForceFeedback);
    GET_USER_FUNC(GamePadGetForceFeedbackState);
    GET_USER_FUNC(GamePadElementHasForceFeedback);
    GET_USER_FUNC(GamePadSetAutocenter);
    GET_USER_FUNC(GamePadGetAutocenter);
    GET_USER_FUNC(GamePadSetFFGain);
    GET_USER_FUNC(GamePadGetFFGain);
    GET_USER_FUNC(GamePadSendForceFeedbackCommand);
    GET_USER_FUNC(GamePadCreateDinputEffect);
    GET_USER_FUNC(GamePadStopAllForceFeedbackEffects);
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

static BOOL CDECL loaderdrv_GamePadHasForceFeedback(int id, DWORD *effects)
{
    return load_driver()->pGamePadHasForceFeedback(id, effects);
}

static BOOL CDECL loaderdrv_GamePadElementHasForceFeedback(int id, int element)
{
    return load_driver()->pGamePadElementHasForceFeedback(id, element);
}

static VOID CDECL loaderdrv_GamePadGetForceFeedbackState(int id, DWORD *state)
{
    load_driver()->pGamePadGetForceFeedbackState(id, state);
}

static HRESULT CDECL loaderdrv_GamePadSetAutocenter(int id, DWORD data)
{
    return load_driver()->pGamePadSetAutocenter(id, data);
}

static HRESULT CDECL loaderdrv_GamePadGetAutocenter(int id, DWORD *data)
{
    return load_driver()->pGamePadGetAutocenter(id, data);
}

static HRESULT CDECL loaderdrv_GamePadSetFFGain(int id, DWORD data)
{
    return load_driver()->pGamePadSetFFGain(id, data);
}

static HRESULT CDECL loaderdrv_GamePadGetFFGain(int id, DWORD *data)
{
    return load_driver()->pGamePadGetFFGain(id, data);
}

static HRESULT CDECL loaderdrv_GamePadSendForceFeedbackCommand(int id, DWORD flags)
{
    return load_driver()->pGamePadSendForceFeedbackCommand(id, flags);
}

static DWORD CDECL loaderdrv_GamePadCreateDinputEffect(int id, const GUID *type, const DIEFFECT *params, IDirectInputEffect **out, IUnknown *outer)
{
    return load_driver()->pGamePadCreateDinputEffect(id, type, params, out, outer);
}

static VOID CDECL loaderdrv_GamePadStopAllForceFeedbackEffects(int id, int release)
{
    load_driver()->pGamePadStopAllForceFeedbackEffects(id, release);
}

static GAMEPAD_DRIVER lazy_load_driver =
{
    loaderdrv_GamePadCount,
    loaderdrv_GamePadName,
    loaderdrv_GamePadElementCount,
    loaderdrv_GamePadElementProps,
    loaderdrv_GamePadPollValues,
    loaderdrv_GamePadAlloc,
    loaderdrv_GamePadDealloc,
    loaderdrv_GamePadHasForceFeedback,
    loaderdrv_GamePadElementHasForceFeedback,
    loaderdrv_GamePadGetForceFeedbackState,
    loaderdrv_GamePadSetAutocenter,
    loaderdrv_GamePadGetAutocenter,
    loaderdrv_GamePadSetFFGain,
    loaderdrv_GamePadGetFFGain,
    loaderdrv_GamePadSendForceFeedbackCommand,
    loaderdrv_GamePadCreateDinputEffect,
    loaderdrv_GamePadStopAllForceFeedbackEffects
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

static BOOL CDECL nulldrv_GamePadHasForceFeedback(int id, DWORD *effects)
{
    return FALSE;
}

static VOID CDECL nulldrv_GamePadGetForceFeedbackState(int id, DWORD *state)
{
    *state = 0;
}

static BOOL CDECL nulldrv_GamePadElementHasForceFeedback(int id, int element)
{
    return FALSE;
}

static HRESULT CDECL nulldrv_GamePadSetAutocenter(int id, DWORD data)
{
    return DIERR_UNSUPPORTED;
}

static HRESULT CDECL nulldrv_GamePadGetAutocenter(int id, DWORD *data)
{
    return DIERR_UNSUPPORTED;
}

static HRESULT CDECL nulldrv_GamePadSetFFGain(int id, DWORD data)
{
    return DIERR_UNSUPPORTED;
}

static HRESULT CDECL nulldrv_GamePadGetFFGain(int id, DWORD *data)
{
    return DIERR_UNSUPPORTED;
}

static HRESULT CDECL nulldrv_GamePadSendForceFeedbackCommand(int id, DWORD flags)
{
    return DI_NOEFFECT;
}

static DWORD CDECL nulldrv_GamePadCreateDinputEffect(int id, const GUID *type, const DIEFFECT *params, IDirectInputEffect **out, IUnknown *outer)
{
    TRACE("No effects support\n");
    *out = NULL;
    return DI_NOEFFECT;
}

static VOID CDECL nulldrv_GamePadStopAllForceFeedbackEffects(int id, int release)
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
    nulldrv_GamePadDealloc,
    nulldrv_GamePadHasForceFeedback,
    nulldrv_GamePadElementHasForceFeedback,
    nulldrv_GamePadGetForceFeedbackState,
    nulldrv_GamePadSetAutocenter,
    nulldrv_GamePadGetAutocenter,
    nulldrv_GamePadSetFFGain,
    nulldrv_GamePadGetFFGain,
    nulldrv_GamePadSendForceFeedbackCommand,
    nulldrv_GamePadCreateDinputEffect,
    nulldrv_GamePadStopAllForceFeedbackEffects
};
