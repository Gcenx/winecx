/*
 * Android driver system tray management
 *
 * Copyright (C) 2004 Mike Hearn, for CodeWeavers
 * Copyright (C) 2005 Robert Shearman
 * Copyright (C) 2008 Alexandre Julliard
 * Copyright (C) 2012, 2013 Ken Thomases for CodeWeavers Inc.
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

#include "android.h"

#include "windef.h"
#include "winuser.h"
#include "shellapi.h"

#include "wine/list.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(systray);

typedef void* android_status_item;

/***********************************************************************
 *   These are communication methods OUT of the systray code
 *   and back into the native platform. They will likely need
 *   to be implemented and moved into other modules with connections
 *   back to the Java driver
 */
void android_destroy_status_item(android_status_item s)
{
    FIXME("UNIMPLEMENTED\n");
}

android_status_item android_create_status_item(void)
{
    FIXME("UNIMPLEMENTED\n");
    return NULL;
}

void android_set_status_item_image(android_status_item s, HICON image)
{
    FIXME("UNIMPLEMENTED\n");
}

void android_set_status_item_tooltip(android_status_item s, WCHAR *cftip)
{
    FIXME("UNIMPLEMENTED\n");
}

/* an individual systray icon */
struct tray_icon
{
    struct list         entry;
    HWND                owner;              /* the HWND passed in to the Shell_NotifyIcon call */
    UINT                id;                 /* the unique id given by the app */
    UINT                callback_message;
    HICON               image;              /* the image to render */
    WCHAR               tiptext[128];       /* tooltip text */
    DWORD               state;              /* state flags */
    android_status_item  status_item;
};

static struct list icon_list = LIST_INIT(icon_list);


static BOOL delete_icon(struct tray_icon *icon);


/***********************************************************************
 *              cleanup_icons
 *
 * Delete all systray icons owned by a given window.
 */
static void cleanup_icons(HWND hwnd)
{
    struct tray_icon *icon, *next;

    LIST_FOR_EACH_ENTRY_SAFE(icon, next, &icon_list, struct tray_icon, entry)
        if (icon->owner == hwnd) delete_icon(icon);
}


/***********************************************************************
 *              get_icon
 *
 * Retrieves an icon record by owner window and ID.
 */
static struct tray_icon *get_icon(HWND owner, UINT id)
{
    struct tray_icon *this;

    LIST_FOR_EACH_ENTRY(this, &icon_list, struct tray_icon, entry)
        if ((this->id == id) && (this->owner == owner)) return this;
    return NULL;
}


/***********************************************************************
 *              modify_icon
 *
 * Modifies an existing tray icon and updates its status item as needed.
 */
static BOOL modify_icon(struct tray_icon *icon, NOTIFYICONDATAW *nid)
{
    BOOL update_image = FALSE, update_tooltip = FALSE;

    TRACE("hwnd %p id 0x%x flags %x\n", nid->hWnd, nid->uID, nid->uFlags);

    if (nid->uFlags & NIF_STATE)
    {
        DWORD changed = (icon->state ^ nid->dwState) & nid->dwStateMask;
        icon->state = (icon->state & ~nid->dwStateMask) | (nid->dwState & nid->dwStateMask);
        if (changed & NIS_HIDDEN)
        {
            if (icon->state & NIS_HIDDEN)
            {
                if (icon->status_item)
                {
                    TRACE("destroying status item %p\n", icon->status_item);
                    android_destroy_status_item(icon->status_item);
                    icon->status_item = NULL;
                }
            }
            else
            {
                if (!icon->status_item)
                {
                    icon->status_item = android_create_status_item();
                    if (icon->status_item)
                    {
                        TRACE("created status item %p\n", icon->status_item);

                        if (icon->image)
                            update_image = TRUE;
                        if (lstrlenW(icon->tiptext))
                            update_tooltip = TRUE;
                    }
                    else
                        WARN("failed to create status item\n");
                }
            }
        }
    }

    if (nid->uFlags & NIF_ICON)
    {
        if (icon->image) DestroyIcon(icon->image);
        icon->image = CopyIcon(nid->hIcon);
        if (icon->status_item)
            update_image = TRUE;
    }

    if (nid->uFlags & NIF_MESSAGE)
    {
        icon->callback_message = nid->uCallbackMessage;
    }
    if (nid->uFlags & NIF_TIP)
    {
        lstrcpynW(icon->tiptext, nid->szTip, sizeof(icon->tiptext)/sizeof(WCHAR));
        if (icon->status_item)
            update_tooltip = TRUE;
    }

    if (update_image)
    {
        android_set_status_item_image(icon->status_item, icon->image);
    }

    if (update_tooltip)
    {
        TRACE("setting tooltip text for status item %p to %s\n", icon->status_item,
              debugstr_w(icon->tiptext));
        android_set_status_item_tooltip(icon->status_item, icon->tiptext);
    }

    return TRUE;
}


/***********************************************************************
 *              add_icon
 *
 * Creates a new tray icon structure and adds it to the list.
 */
static BOOL add_icon(NOTIFYICONDATAW *nid)
{
    NOTIFYICONDATAW new_nid;
    struct tray_icon *icon;

    TRACE("hwnd %p id 0x%x\n", nid->hWnd, nid->uID);

    if ((icon = get_icon(nid->hWnd, nid->uID)))
    {
        WARN("duplicate tray icon add, buggy app?\n");
        return FALSE;
    }

    if (!(icon = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*icon))))
    {
        ERR("out of memory\n");
        return FALSE;
    }

    icon->id     = nid->uID;
    icon->owner  = nid->hWnd;
    icon->state  = NIS_HIDDEN;

    list_add_tail(&icon_list, &icon->entry);

    if (!(nid->uFlags & NIF_STATE) || !(nid->dwStateMask & NIS_HIDDEN))
    {
        new_nid = *nid;
        new_nid.uFlags |= NIF_STATE;
        new_nid.dwState     &= ~NIS_HIDDEN;
        new_nid.dwStateMask |= NIS_HIDDEN;
        nid = &new_nid;
    }
    return modify_icon(icon, nid);
}


/***********************************************************************
 *              delete_icon
 *
 * Destroy tray icon status item and delete structure.
 */
static BOOL delete_icon(struct tray_icon *icon)
{
    TRACE("hwnd %p id 0x%x\n", icon->owner, icon->id);

    if (icon->status_item)
    {
        TRACE("destroying status item %p\n", icon->status_item);
        android_destroy_status_item(icon->status_item);
    }
    list_remove(&icon->entry);
    DestroyIcon(icon->image);
    HeapFree(GetProcessHeap(), 0, icon);
    return TRUE;
}


/***********************************************************************
 *              wine_notify_icon   (MACDRV.@)
 *
 * Driver-side implementation of Shell_NotifyIcon.
 */
int CDECL ANDROID_notify_icon(DWORD msg, NOTIFYICONDATAW *data)
{
    BOOL ret = FALSE;
    struct tray_icon *icon;

    switch (msg)
    {
    case NIM_ADD:
        ret = add_icon(data);
        break;
    case NIM_DELETE:
        if ((icon = get_icon(data->hWnd, data->uID))) ret = delete_icon(icon);
        break;
    case NIM_MODIFY:
        if ((icon = get_icon(data->hWnd, data->uID))) ret = modify_icon(icon, data);
        break;
    case 0xdead:  /* Wine extension: owner window has died */
        cleanup_icons(data->hWnd);
        break;
    default:
        FIXME("unhandled tray message: %u\n", msg);
        break;
    }
    return ret;
}


/***********************************************************************
 * These are methods coming INTO the systray code from the platform
 * driver
 */

/***********************************************************************
 *              android_status_item_mouse_button
 *
 * Handle STATUS_ITEM_MOUSE_BUTTON events.
 */
void android_status_item_mouse_button(const union event_data *event)
{
    FIXME("UNIMPLEMENTED\n");
}


/***********************************************************************
 *              android_status_item_mouse_move
 *
 * Handle STATUS_ITEM_MOUSE_MOVE events.
 */
void android_status_item_mouse_move(const union event_data *event)
{
    FIXME("UNIMPLEMENTED\n");
}
