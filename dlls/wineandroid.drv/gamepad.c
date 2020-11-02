/*
 * The interface with the Android Gamepads
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

#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "wine/debug.h"
#include "imm.h"
#include "ddk/imm.h"
#include "winnls.h"
#include "android.h"

WINE_DEFAULT_DEBUG_CHANNEL(android);

di_value_set *di_value = NULL;
di_name      *di_names = NULL;
int          di_controllers = 0;

static int *di_ids = NULL;
static int di_controller_alloc = 0;

/*
 * All the following functions run in the Android process
 */
static int get_index_from_id(int device)
{
    int dev_index = 0;
    for (dev_index = 0; dev_index < di_controllers; dev_index++)
        if (device == di_ids[dev_index])
            break;
    if (dev_index >=di_controllers)
        return -1;

    return dev_index;
}

void gamepad_count( JNIEnv *env, jobject obj, jint count )
{
    if (count > di_controller_alloc)
    {
        int i;
        di_ids = realloc(di_ids, sizeof(int) * count);
        di_names = realloc(di_names, sizeof(di_name) * count);
        di_value = realloc(di_value, sizeof(di_value_set) * count);
        for (i = di_controller_alloc; i < count; i++)
        {
            di_ids[i] = 0;
            di_names[i][0] = '\0';
            memset(&di_value[i], 0, sizeof(di_value_set));
        }
        di_controller_alloc = count;
    }
    di_controllers = count;
}

void gamepad_data( JNIEnv *env, jobject obj, jint index, jint id, jstring name)
{
    const jchar* _name;
    if (index > di_controllers)
        return;

    _name = (*env)->GetStringChars(env, name, 0);
    lstrcpynW(di_names[index], (WCHAR*)_name, DI_NAME_LENGTH);
    (*env)->ReleaseStringChars(env, name, _name);
    di_ids[index] = id;
}

void gamepad_sendbutton( JNIEnv *env, jobject obj, jint device, jint key, jint value)
{
    int dev_index = get_index_from_id(device);
    if (dev_index < 0)
        return;

    if ( key >= 96 && key <= 110)
    di_value[dev_index][(key-96)+DI_BUTTON_DATA_OFFSET] = value;
    else if (key >= 188 && key <= 203)
    di_value[dev_index][(key-188)+DI_BUTTON_DATA_OFFSET+14] = value;
}

void gamepad_sendaxis(JNIEnv *env, jobject obj, jint device, jfloatArray axis)
{
    int i;
    int dev_index = get_index_from_id(device);
    jfloat *values;

    if (dev_index < 0)
        return;

    values = (*env)->GetFloatArrayElements(env, axis, NULL);
    for (i = 0; i < DI_AXIS_DATA_COUNT; i++)
        di_value[dev_index][i] = (int)(0xffff * values[i]);
    (*env)->ReleaseFloatArrayElements(env, axis, values, JNI_ABORT);
}
/*
 * End Android process functions
 */

/*
 * All the following functions run in WINE processes
 */
INT CDECL GamePadCount(void)
{
    int count;
    ioctl_gamepad_query(0, 0, &count);
    return count;
}

VOID CDECL GamePadName(int id, char* name, int len)
{
    WCHAR _name[DI_NAME_LENGTH];
    ioctl_gamepad_query(1, id, _name);
    WideCharToMultiByte(CP_UTF8, 0, _name, -1, name, len, NULL, NULL);
}

VOID CDECL GamePadElementCount(int id, DWORD *axis, DWORD *buttons, DWORD *povs, int axis_map[8])
{
    int i;
    *axis = DI_AXIS_COUNT;
    *povs = DI_POV_COUNT;
    *buttons = DI_BUTTON_COUNT;
    for (i = 0; i < DI_AXIS_COUNT; i++)
        axis_map[i] = min(i,6);
}

VOID CDECL GamePadElementProps(int id, int element, int *min, int *max)
{
    *min = -1 * 0xffff;
    *max = 0xffff;
}

VOID CDECL GamePadPollValues(int id, int *values)
{
    ioctl_gamepad_query(2, id, values);
}
/*
 * End WINE process functions
 */
