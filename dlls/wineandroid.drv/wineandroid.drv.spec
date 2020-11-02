# GDI driver

@ cdecl wine_get_gdi_driver(long) ANDROID_get_gdi_driver

# USER driver

## @ cdecl ActivateKeyboardLayout(long long) ANDROID_ActivateKeyboardLayout
## @ cdecl Beep() ANDROID_Beep
@ cdecl GetKeyNameText(long ptr long) ANDROID_GetKeyNameText
@ cdecl GetKeyboardLayout(long) ANDROID_GetKeyboardLayout
## @ cdecl GetKeyboardLayoutName(ptr) ANDROID_GetKeyboardLayoutName
## @ cdecl LoadKeyboardLayout(wstr long) ANDROID_LoadKeyboardLayout
@ cdecl MapVirtualKeyEx(long long long) ANDROID_MapVirtualKeyEx
@ cdecl ToUnicodeEx(long long ptr ptr long long long) ANDROID_ToUnicodeEx
## @ cdecl UnloadKeyboardLayout(long) ANDROID_UnloadKeyboardLayout
@ cdecl VkKeyScanEx(long long) ANDROID_VkKeyScanEx
## @ cdecl DestroyCursorIcon(long) ANDROID_DestroyCursorIcon
@ cdecl SetCursor(long) ANDROID_SetCursor
## @ cdecl GetCursorPos(ptr) ANDROID_GetCursorPos
## @ cdecl SetCursorPos(long long) ANDROID_SetCursorPos
## @ cdecl ClipCursor(ptr) ANDROID_ClipCursor
@ cdecl ChangeDisplaySettingsEx(ptr ptr long long long) ANDROID_ChangeDisplaySettingsEx
@ cdecl EnumDisplayMonitors(long ptr ptr long) ANDROID_EnumDisplayMonitors
@ cdecl EnumDisplaySettingsEx(ptr long ptr long) ANDROID_EnumDisplaySettingsEx
@ cdecl GetMonitorInfo(long ptr) ANDROID_GetMonitorInfo
## @ cdecl CreateDesktopWindow(long) ANDROID_CreateDesktopWindow
@ cdecl CreateWindow(long) ANDROID_CreateWindow
@ cdecl DestroyWindow(long) ANDROID_DestroyWindow
## @ cdecl GetDC(long long long ptr ptr long) ANDROID_GetDC
@ cdecl MsgWaitForMultipleObjectsEx(long ptr long long long) ANDROID_MsgWaitForMultipleObjectsEx
## @ cdecl ReleaseDC(long long) ANDROID_ReleaseDC
## @ cdecl ScrollDC(long long long ptr ptr long ptr) ANDROID_ScrollDC
@ cdecl SetCapture(long long) ANDROID_SetCapture
@ cdecl SetFocus(long) ANDROID_SetFocus
@ cdecl SetLayeredWindowAttributes(long long long long) ANDROID_SetLayeredWindowAttributes
@ cdecl SetParent(long long long) ANDROID_SetParent
@ cdecl SetWindowIcon(long long long) ANDROID_SetWindowIcon
@ cdecl SetWindowRgn(long long long) ANDROID_SetWindowRgn
@ cdecl SetWindowStyle(ptr long ptr) ANDROID_SetWindowStyle
@ cdecl SetWindowText(long wstr) ANDROID_SetWindowText
@ cdecl ShowWindow(long long ptr long) ANDROID_ShowWindow
## @ cdecl SysCommand(long long long) ANDROID_SysCommand
## @ cdecl ThreadDetach() ANDROID_ThreadDetach
@ cdecl UpdateClipboard() ANDROID_UpdateClipboard
@ cdecl UpdateLayeredWindow(long ptr ptr) ANDROID_UpdateLayeredWindow
@ cdecl WindowMessage(long long long long) ANDROID_WindowMessage
@ cdecl WindowPosChanging(long long long ptr ptr ptr ptr) ANDROID_WindowPosChanging
@ cdecl WindowPosChanged(long long long ptr ptr ptr ptr ptr) ANDROID_WindowPosChanged
## @ cdecl SystemParametersInfo(long long ptr long) ANDROID_SystemParametersInfo

# WinTab32
## @ cdecl AttachEventQueueToTablet(long) ANDROID_AttachEventQueueToTablet
## @ cdecl GetCurrentPacket(ptr) ANDROID_GetCurrentPacket
## @ cdecl LoadTabletInfo(long) ANDROID_LoadTabletInfo
## @ cdecl WTInfoW(long long ptr) ANDROID_WTInfoW

# Desktop
@ cdecl wine_create_desktop(long long) ANDROID_create_desktop

# System tray
@ cdecl wine_notify_icon(long ptr) ANDROID_notify_icon

#IME Interface
@ stdcall ImeInquire(ptr ptr wstr)
@ stdcall ImeConfigure(long long long ptr)
@ stdcall ImeDestroy(long)
@ stdcall ImeEscape(long long ptr)
@ stdcall ImeSelect(long long)
@ stdcall ImeSetActiveContext(long long)
@ stdcall ImeToAsciiEx(long long ptr ptr long long)
@ stdcall NotifyIME(long long long long)
@ stdcall ImeRegisterWord(wstr long wstr)
@ stdcall ImeUnregisterWord(wstr long wstr)
@ stdcall ImeEnumRegisterWord(ptr wstr long wstr ptr)
@ stdcall ImeSetCompositionString(long long ptr long ptr long)
@ stdcall ImeConversionList(long wstr ptr long long)
@ stdcall ImeProcessKey(long long long ptr)
@ stdcall ImeGetRegisterWordStyle(long ptr)
@ stdcall ImeGetImeMenuItems(long long long ptr ptr long)

# MMDevAPI driver functions
@ stdcall -private GetPriority() AUDDRV_GetPriority
@ stdcall -private GetEndpointIDs(long ptr ptr ptr ptr) AUDDRV_GetEndpointIDs
@ stdcall -private GetAudioEndpoint(ptr ptr ptr) AUDDRV_GetAudioEndpoint
@ stdcall -private GetAudioSessionManager(ptr ptr) AUDDRV_GetAudioSessionManager

# dinput driver functions
@ stdcall -private GamePadCount()
@ stdcall -private GamePadName(long ptr long)
@ stdcall -private GamePadElementCount(long ptr ptr ptr ptr)
@ stdcall -private GamePadElementProps(long long ptr ptr)
@ stdcall -private GamePadPollValues(long ptr)
