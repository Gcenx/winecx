/*
 * Android driver definitions
 *
 * Copyright 2013 Alexandre Julliard
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

#ifndef __WINE_ANDROID_H
#define __WINE_ANDROID_H

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <jni.h>
#include <android/log.h>
#include <android/input.h>
#include <android/native_window_jni.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/gdi_driver.h"
#include "android_native.h"


/**************************************************************************
 * Android interface
 */

#define DECL_FUNCPTR(f) extern typeof(f) * p##f DECLSPEC_HIDDEN
DECL_FUNCPTR( __android_log_print );
DECL_FUNCPTR( ANativeWindow_fromSurface );
DECL_FUNCPTR( ANativeWindow_release );
#undef DECL_FUNCPTR


/**************************************************************************
 * OpenGL driver
 */

extern void update_gl_drawable( HWND hwnd ) DECLSPEC_HIDDEN;
extern void destroy_gl_drawable( HWND hwnd ) DECLSPEC_HIDDEN;
extern struct opengl_funcs *get_wgl_driver( UINT version ) DECLSPEC_HIDDEN;


/**************************************************************************
 * Android pseudo-device
 */

extern void start_android_device(void) DECLSPEC_HIDDEN;
extern void register_native_window( HWND hwnd, struct ANativeWindow *win, BOOL client ) DECLSPEC_HIDDEN;
extern struct ANativeWindow *create_ioctl_window( HWND hwnd, BOOL opengl, float scale ) DECLSPEC_HIDDEN;
extern struct ANativeWindow *grab_ioctl_window( struct ANativeWindow *window ) DECLSPEC_HIDDEN;
extern void release_ioctl_window( struct ANativeWindow *window ) DECLSPEC_HIDDEN;
extern void destroy_ioctl_window( HWND hwnd, BOOL opengl ) DECLSPEC_HIDDEN;
extern int ioctl_window_pos_changed( HWND hwnd, const RECT *window_rect, const RECT *client_rect,
                                     const RECT *visible_rect, UINT style, UINT flags,
                                     HWND after, HWND owner ) DECLSPEC_HIDDEN;
extern int ioctl_set_window_parent( HWND hwnd, HWND parent, float scale ) DECLSPEC_HIDDEN;
extern int ioctl_set_window_focus( HWND hwnd ) DECLSPEC_HIDDEN;
extern int ioctl_set_window_text( HWND hwnd, const WCHAR *text ) DECLSPEC_HIDDEN;
extern int ioctl_set_window_icon( HWND hwnd, int width, int height,
                                  const unsigned int *bits ) DECLSPEC_HIDDEN;
extern int ioctl_set_capture( HWND hwnd ) DECLSPEC_HIDDEN;
extern int ioctl_set_cursor( int id, int width, int height,
                             int hotspotx, int hotspoty, const unsigned int *bits ) DECLSPEC_HIDDEN;
extern int ioctl_gamepad_query( int index, int device, void* data) DECLSPEC_HIDDEN;
extern int ioctl_imeText( int target, int *cursor, int *length, WCHAR* string ) DECLSPEC_HIDDEN;
extern int ioctl_imeFinish( int target ) DECLSPEC_HIDDEN;
extern int ioctl_poll_clipdata( void ) DECLSPEC_HIDDEN;
extern int ioctl_get_clipdata( DWORD flags, LPWSTR* result ) DECLSPEC_HIDDEN;
extern int ioctl_set_clipdata( const WCHAR* text ) DECLSPEC_HIDDEN;
extern int ioctl_close_desktop(void) DECLSPEC_HIDDEN;


/**************************************************************************
 * USER driver
 */

extern unsigned int screen_width DECLSPEC_HIDDEN;
extern unsigned int screen_height DECLSPEC_HIDDEN;
extern unsigned int screen_dpi DECLSPEC_HIDDEN;
extern RECT virtual_screen_rect DECLSPEC_HIDDEN;
extern MONITORINFOEXW default_monitor DECLSPEC_HIDDEN;

enum android_window_messages
{
    WM_ANDROID_REFRESH = 0x80001000,
    WM_ANDROID_IME_CONTROL,
    WM_ANDROID_SET_FOCUS,
};

extern void init_gralloc( const struct hw_module_t *module ) DECLSPEC_HIDDEN;
extern HWND get_capture_window(void) DECLSPEC_HIDDEN;
extern void init_monitors( int width, int height ) DECLSPEC_HIDDEN;
extern void set_screen_dpi( DWORD dpi ) DECLSPEC_HIDDEN;
extern void handle_run_cmdline( LPWSTR cmdline, LPWSTR* wineEnv ) DECLSPEC_HIDDEN;
extern void handle_run_cmdarray( LPWSTR* cmdline, LPWSTR* wineEnv ) DECLSPEC_HIDDEN;
extern void handle_clear_meta_key_states( int states ) DECLSPEC_HIDDEN;
extern void get_exported_formats( BOOL* formats, int num_formats ) DECLSPEC_HIDDEN;
extern void update_keyboard_lock_state( WORD vkey, UINT state ) DECLSPEC_HIDDEN;
extern void init_clipboard(void) DECLSPEC_HIDDEN;

/* JNI entry points */
extern void desktop_changed( JNIEnv *env, jobject obj, jint width, jint height ) DECLSPEC_HIDDEN;
extern void config_changed( JNIEnv *env, jobject obj, jint dpi ) DECLSPEC_HIDDEN;
extern void surface_changed( JNIEnv *env, jobject obj, jint win, jobject surface,
                             jboolean client ) DECLSPEC_HIDDEN;
extern jboolean motion_event( JNIEnv *env, jobject obj, jint win, jint action,
                              jint x, jint y, jint state, jint vscroll ) DECLSPEC_HIDDEN;
extern jboolean keyboard_event( JNIEnv *env, jobject obj, jint win, jint action,
                                jint keycode, jint state ) DECLSPEC_HIDDEN;
extern jboolean clear_meta_key_states( JNIEnv *env, jobject obj, jint states ) DECLSPEC_HIDDEN;
extern void run_commandline( JNIEnv *env, jobject obj, jobject _cmdline, jobjectArray _wineEnv ) DECLSPEC_HIDDEN;;
extern void run_commandarray( JNIEnv *env, jobject obj, jobjectArray _cmdarray, jobjectArray _wineEnv ) DECLSPEC_HIDDEN;;
extern void set_focus( JNIEnv *env, jobject obj, jint win ) DECLSPEC_HIDDEN;
extern void send_syscommand( JNIEnv *env, jobject obj, jint win, jint param ) DECLSPEC_HIDDEN;
extern void send_window_close( JNIEnv *env, jobject obj, jint win ) DECLSPEC_HIDDEN;

/* Clipboard entry points and data */
#define ANDROID_CLIPDATA_OURS 0x1
#define ANDROID_CLIPDATA_HASTEXT 0x2
#define ANDROID_CLIPDATA_EMPTYTEXT 0x4
#define ANDROID_CLIPDATA_SPANNEDTEXT 0x8
#define ANDROID_CLIPDATA_HASHTML 0x10
#define ANDROID_CLIPDATA_HASINTENT 0x20
#define ANDROID_CLIPDATA_HASURI 0x40

extern void clipdata_update( JNIEnv *env, jobject obj, jint flags, jobjectArray mimetypes ) DECLSPEC_HIDDEN;
extern void handle_clipdata_update( DWORD flags, LPWSTR *mimetypes ) DECLSPEC_HIDDEN;

/* IME entry points */
extern void IME_UpdateAssociation(HWND focus) DECLSPEC_HIDDEN;
extern void ime_text( JNIEnv *env, jobject obj, jstring text, jint length, jint cursor) DECLSPEC_HIDDEN;
extern void ime_finish( JNIEnv *env, jobject obj) DECLSPEC_HIDDEN;
extern void ime_cancel( JNIEnv *env, jobject obj) DECLSPEC_HIDDEN;
extern void ime_start( JNIEnv *env, jobject obj) DECLSPEC_HIDDEN;
extern void handle_IME_TEXT(int target, int length) DECLSPEC_HIDDEN;
extern void handle_IME_FINISH(int target, int length) DECLSPEC_HIDDEN;
extern void handle_IME_CANCEL(void) DECLSPEC_HIDDEN;
extern void handle_IME_START(void) DECLSPEC_HIDDEN;
extern LRESULT Ime_Control(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) DECLSPEC_HIDDEN;

/* GAMEPAD entry points and DATA*/
extern void gamepad_count(JNIEnv *env, jobject obj, jint count) DECLSPEC_HIDDEN;
extern void gamepad_data(JNIEnv *env, jobject obj, jint index, jint id, jstring name) DECLSPEC_HIDDEN;
extern void gamepad_sendaxis(JNIEnv *env, jobject obj, jint device, jfloatArray axis) DECLSPEC_HIDDEN;
extern void gamepad_sendbutton(JNIEnv *env, jobject obj, jint device, jint element, jint value) DECLSPEC_HIDDEN;

#define DI_AXIS_COUNT 8
#define DI_POV_COUNT 1  /* POVs take 2 axis  */
#define DI_AXIS_DATA_COUNT (DI_AXIS_COUNT + DI_POV_COUNT*2)
#define DI_BUTTON_COUNT 30
#define DI_DATASIZE  (DI_AXIS_DATA_COUNT + DI_BUTTON_COUNT)
#define DI_BUTTON_DATA_OFFSET DI_AXIS_DATA_COUNT
#define DI_NAME_LENGTH 255

typedef int di_value_set[DI_DATASIZE];
typedef WCHAR di_name[DI_NAME_LENGTH];

extern di_value_set *di_value;
extern di_name      *di_names;
extern int          di_controllers;

enum event_type
{
    DESKTOP_CHANGED,
    CONFIG_CHANGED,
    SURFACE_CHANGED,
    MOTION_EVENT,
    KEYBOARD_EVENT,
    SET_FOCUS,
    SEND_SYSCOMMAND,
    IME_TEXT,
    IME_FINISH,
    IME_CANCEL,
    IME_START,
    RUN_CMDLINE,
    RUN_CMDARRAY,
    CLEAR_META,
    CLIPDATA_UPDATE,
    SEND_WINDOW_CLOSE
};

union event_data
{
    enum event_type type;
    struct
    {
        enum event_type type;
        unsigned int    width;
        unsigned int    height;
    } desktop;
    struct
    {
        enum event_type type;
        unsigned int    dpi;
    } cfg;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        ANativeWindow  *window;
        BOOL            client;
        unsigned int    width;
        unsigned int    height;
    } surface;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        INPUT           input;
    } motion;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        UINT            lock_state;
        INPUT           input;
    } kbd;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        RECT            rect;
    } flush;
    struct
    {
        enum event_type type;
        HWND            hwnd;
    } focus;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        WPARAM          wp;
    } syscommand;
    struct
    {
        enum event_type type;
        INT             android_format;
        DWORD           flags;
        LPWSTR*         mimetypes;
    } clipdata;
    struct
    {
        enum event_type type;
        WORD            target;
        WORD            length;
    } ime_text;
    struct
    {
        enum event_type type;
        WORD            target;
        WORD            length;
    } ime_finish;
    struct
    {
        enum event_type type;
        LPWSTR          cmdline;
        LPWSTR*         env;
    } runcmd;
    struct
    {
        enum event_type type;
        int             states;
    } clearmeta;
    struct
    {
        enum event_type type;
        LPWSTR*         cmdarray;
        LPWSTR*         env;
    } runcmdarr;
};

int send_event( const union event_data *data ) DECLSPEC_HIDDEN;

extern JavaVM *wine_get_java_vm(void);
extern jobject wine_get_java_object(void);

typedef struct _s_ime_text {
    WCHAR *text;
    INT    length;
    INT    cursor_pos;
} s_ime_text;

extern s_ime_text **java_ime_text;
extern INT java_ime_active_target;
extern INT java_ime_count;

#endif  /* __WINE_ANDROID_H */
