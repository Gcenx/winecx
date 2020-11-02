/*
 * Functions for further XIM control on the Mac intel OS X platform
 *
 * Copyright 2006,2008 CodeWeavers, Aric Stewart
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

#ifdef __APPLE__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* X includes */
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

/* Wine Includes */
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wingdi.h"
#include "winnls.h"
#include "imm.h"
#include "ddk/imm.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mac_ime);

int ProcessMacInput(XKeyEvent *event)
{
    HWND wnd;
    static HIMC hImc = NULL;
    HIMC newImc = NULL;
	KeySym keysym;

	if (event->type != KeyPress)
		return 0;

    wnd = GetFocus();
    newImc = ImmGetContext(wnd);

    if (newImc != hImc)
    {
        hImc = newImc;
        if (ImmGetIMEFileNameA(GetKeyboardLayout(0),NULL,0))
        {
            /* We have a non default IME  do not do our processing */
            return 0;
        }
    }

    if (hImc)
    {
	    XLookupString(event, NULL, 0, &keysym, NULL);
        ImmEscapeW(GetKeyboardLayout(0),hImc,IME_ESC_PRIVATE_FIRST + 0x10, (LPVOID)keysym);
        ImmEscapeW(GetKeyboardLayout(0),hImc,IME_ESC_PRIVATE_FIRST + 0x11, (LPVOID)(event->keycode-8));
    }
    return 0;
}

#endif /* __APPLE__ */
