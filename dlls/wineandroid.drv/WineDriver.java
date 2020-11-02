/*
 * WineDriver class
 *
 * Copyright 2013-2017 Alexandre Julliard
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

package org.winehq.wine;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.ClipboardManager;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ContentProvider;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.text.Spanned;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.TextView;
import android.widget.Toast;
import java.lang.ClassNotFoundException;
import java.lang.ProcessBuilder;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.lang.StringBuilder;
import java.nio.charset.Charset;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Map;
import java.util.Map.Entry;

import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.CorrectionInfo;
import android.text.Spannable;
import android.text.Editable;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.ExtractedText;
import android.text.InputType;
import android.view.KeyCharacterMap;

public class WineDriver extends Object implements ClipboardManager.OnPrimaryClipChangedListener
{
    private native String wine_init( String[] cmdline, String[] env );

    private final String LOGTAG = "wine";

    protected Activity activity;
    protected Context context;
    private TextView progressStatusText;
    private int startupDpi;
    private boolean firstRun;
    private Handler mainThreadHandler;
    private ClipboardManager clipboard_manager;
    private ClipData clipdata;
    private PointerIcon current_cursor;
    private static ArrayList<WineCmdLine> queuedCmdlines = new ArrayList<WineCmdLine>();
    private static ArrayList<WineCmdArray> queuedCmdarrays = new ArrayList<WineCmdArray>();
    private static final Object desktopReadyLock = new Object();
    private static Boolean desktopReady = false;
    private File prefix = null;
    protected float dpi_scale = 1.0f;

    private String providerAuthority;

    static private final int CLIPDATA_OURS = 0x1;
    static private final int CLIPDATA_HASTEXT = 0x2;
    static private final int CLIPDATA_EMPTYTEXT = 0x4;
    static private final int CLIPDATA_SPANNEDTEXT = 0x8;
    static private final int CLIPDATA_HASHTML = 0x10;
    static private final int CLIPDATA_HASINTENT = 0x20;
    static private final int CLIPDATA_HASURI = 0x40;

    private class WineCmdLine
    {
        public String cmdline;
        public HashMap<String,String> env;

        public WineCmdLine( String _cmdline, HashMap<String,String> _env)
        {
            cmdline = _cmdline;
            env = _env;
        }
    }

    private class WineCmdArray
    {
        public String[] cmdarray;
        public HashMap<String,String> env;

        public WineCmdArray( String[] _cmdarray, HashMap<String,String> _env)
        {
            cmdarray = _cmdarray;
            env = _env;
        }
    }

    public WineDriver( Context c, Activity act )
    {
        context = c;
        activity = act;
    }

    private Activity getActivity()
    {
        return activity;
    }

    private void runOnUiThread( Runnable r )
    {
        if (mainThreadHandler == null)
            mainThreadHandler = new Handler( Looper.getMainLooper() );
        mainThreadHandler.post( r );
    }

    private final void runWine( String libwine, HashMap<String,String> wineEnv, String[] cmdline )
    {
        System.load( libwine );

        String binPath = new File( wineEnv.get( "WINELOADER" ) ).getParentFile().getAbsolutePath();
        String wineserver = binPath + "/wineserver";
        Log.i( LOGTAG, "New explorer process: shutting down prefix " + wineEnv.get( "WINEPREFIX" ) );
        try
        {
            ProcessBuilder pb = new ProcessBuilder( wineserver, "-k" );
            Map<String,String> shutdownEnv = pb.environment();
            shutdownEnv.put( "WINEPREFIX", wineEnv.get( "WINEPREFIX" ) );
            java.lang.Process p = pb.start();
            p.waitFor();
        }
        catch (Exception e)
        {
            Log.i( LOGTAG, "Prefix shutdown failed: " + e );
        }

        prefix = new File( wineEnv.get( "WINEPREFIX" ) );
        prefix.mkdirs();

        String[] env = new String[wineEnv.size() * 2];
        int j = 0;
        for (Map.Entry<String,String> entry : wineEnv.entrySet())
        {
            env[j++] = entry.getKey();
            env[j++] = entry.getValue();
        }

        String[] cmd = new String[3 + cmdline.length];
        cmd[0] = wineEnv.get( "WINELOADER" );
        cmd[1] = "explorer.exe";
        cmd[2] = "/desktop=" + get_desktop_name() + ",,android";
        System.arraycopy( cmdline, 0, cmd, 3, cmdline.length );

        String cmd_str = "";
        for (String s : cmd) cmd_str += " " + s;
        Log.i( LOGTAG, "Running startup program:" + cmd_str );

        String err = wine_init( cmd, env );
        Log.e( LOGTAG, err );
    }

    public void setStartupDpi( int dpi )
    {
        startupDpi = dpi;
    }

    public void loadWine( final String libwine, final HashMap<String,String> wineEnv, final String[] cmdline )
    {
        firstRun = !(new File ( wineEnv.get( "WINEPREFIX" ) ).exists());
        String existing = firstRun ? "new" : "existing";
        Log.i( LOGTAG, "Initializing wine in " + existing + " prefix " + wineEnv.get( "WINEPREFIX" ) );
        Runnable main_thread = new Runnable() { public void run() { runWine( libwine, wineEnv, cmdline ); } };
        new Thread( main_thread ).start();
    }

    public void runCmdline( final String cmdline, HashMap<String,String> envMap )
    {
        synchronized( desktopReadyLock )
        {
            if (desktopReady)
            {
                String[] env = null;
                Log.i( LOGTAG, "Running command line: " + cmdline );
                if (cmdline == null) return;

                if (envMap != null)
                {
                    int j = 0;
                    env = new String[envMap.size() * 2];
                    for (Map.Entry<String,String> entry : envMap.entrySet())
                    {
                        env[j++] = entry.getKey();
                        env[j++] = entry.getValue();
                    }
                }

                wine_run_commandline( cmdline, env );
            }
            else
            {
                Log.i( LOGTAG, "Desktop not yet ready: queueing cmdline " + cmdline );
                queuedCmdlines.add( new WineCmdLine( cmdline, envMap ) );
            }
        }
    }

    public void runCmdarray( final String[] cmdarray, HashMap<String,String> envMap )
    {
        synchronized( desktopReadyLock )
        {
            String[] env = null;
            StringBuilder sb = new StringBuilder();
            for (String cmd: cmdarray)
                sb.append( cmd + " " );

            if (desktopReady)
            {
                Log.i( LOGTAG, "Running command array: " + sb.toString() );
                if (cmdarray == null) return;

                if (envMap != null)
                {
                    int j = 0;
                    env = new String[envMap.size() * 2];
                    for (Map.Entry<String,String> entry : envMap.entrySet())
                    {
                        env[j++] = entry.getKey();
                        env[j++] = entry.getValue();
                    }
                }

                wine_run_commandarray( cmdarray, env );
            }
            else
            {
                Log.i( LOGTAG, "Desktop not yet ready: queueing cmdarray " + sb.toString());
                queuedCmdarrays.add( new WineCmdArray( cmdarray, envMap ) );
            }
        }
    }

    public void closeDesktop()
    {
        if (activity != null) activity.finish();
    }

    public void setProviderAuthority( final String authority )
    {
        this.providerAuthority = authority;
    }

    protected String get_desktop_name()
    {
        return "shell";
    }

    private final void updateGamepads()
    {
        ArrayList<Integer> gameControllerDeviceIds = new ArrayList<Integer>();
        ArrayList<String> gameControllerDeviceNames = new ArrayList<String>();
        int[] deviceIds = InputDevice.getDeviceIds();

        for (int deviceId : deviceIds)
        {
            InputDevice dev = InputDevice.getDevice(deviceId);
            int sources = dev.getSources();

            if (((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD)
                || ((sources & InputDevice.SOURCE_JOYSTICK)
                    == InputDevice.SOURCE_JOYSTICK))
            {
                if (!gameControllerDeviceIds.contains(deviceId))
                {
                    gameControllerDeviceIds.add(deviceId);
                    gameControllerDeviceNames.add(dev.getDescriptor());
                }
            }
        }

        if (gameControllerDeviceIds.size() > 0)
        {
            int count = gameControllerDeviceIds.size();
            int i;

            wine_send_gamepad_count( count );
            for (i = 0; i < count; i++)
            {
                int id = gameControllerDeviceIds.get(i);
                String name = gameControllerDeviceNames.get(i);
                wine_send_gamepad_data(i, id, name);
            }
        }
    }

    public native void wine_send_gamepad_count(int count);
    public native void wine_send_gamepad_data(int index, int id, String name);
    public native void wine_send_gamepad_axis(int device, float[] axis);
    public native void wine_send_gamepad_button(int device, int button, int value);

    public native void wine_ime_start();
    public native void wine_ime_settext( String text, int length, int cursor );
    public native void wine_ime_finishtext();
    public native void wine_ime_canceltext();

    protected class WineInputConnection extends BaseInputConnection
    {
        KeyCharacterMap mKeyCharacterMap;

        public WineInputConnection( WineView targetView )
        {
            super( targetView, true);
        }

        public boolean beginBatchEdit()
        {
            Log.i(LOGTAG, "beginBatchEdit");
            return true;
        }

        public boolean clearMetaKeyStates (int states)
        {
            Log.i(LOGTAG, "clearMetaKeyStates");
            wine_clear_meta_key_states( states );
            return super.clearMetaKeyStates (states);
        }

        public boolean commitCompletion (CompletionInfo text)
        {
            Log.i(LOGTAG, "commitCompletion");
            return super.commitCompletion (text);
        }

        public boolean commitCorrection (CorrectionInfo correctionInfo)
        {
            Log.i(LOGTAG, "commitCorrection");
            return super.commitCorrection (correctionInfo);
        }

        public boolean commitText (CharSequence text, int newCursorPosition)
        {
            Log.i(LOGTAG, "commitText: '"+text.toString()+"'");

            super.commitText (text, newCursorPosition);

            /* This code based on BaseInputConnection in order to generate
                keystroke events for single character mappable input */
            Editable content = getEditable();
            if (content != null)
            {
                final int N = content.length();
                if (N == 1)
                {
                    // If it's 1 character, we have a chance of being
                    // able to generate normal key events...
                    if (mKeyCharacterMap == null)
                    {
                        mKeyCharacterMap = KeyCharacterMap.load(
                                KeyCharacterMap.BUILT_IN_KEYBOARD);
                    }
                    char[] chars = new char[1];
                    content.getChars(0, 1, chars, 0);
                    KeyEvent[] events = mKeyCharacterMap.getEvents(chars);
                    if (events != null)
                    {
                        wine_ime_canceltext();
                        for (int i=0; i<events.length; i++)
                        {
                            sendKeyEvent(events[i]);
                        }
                        content.clear();
                        return true;
                    }
                }

                wine_ime_start();
                wine_ime_settext( content.toString(), content.length(), newCursorPosition );
                wine_ime_finishtext();
                content.clear();
            }
            return true;
        }

        public boolean deleteSurroundingText (int beforeLength, int afterLength)
        {
            Log.i(LOGTAG, "deleteSurroundingText :"+beforeLength+","+afterLength);
            super.deleteSurroundingText (beforeLength, afterLength);
            return true;
        }

        public boolean endBatchEdit ()
        {
            Log.i(LOGTAG, "endBatchEdit");
            return true;
        }

        public boolean finishComposingText ()
        {
            Log.i(LOGTAG, "finishComposingText");
            wine_ime_finishtext();
            Editable content = getEditable();
            if (content != null)
                content.clear();
            return super.finishComposingText ();
        }

        public int getCursorCapsMode (int reqModes)
        {
            Log.i(LOGTAG, "getCursorCapsMode");
            return super.getCursorCapsMode (reqModes);
        }

        public Editable getEditable ()
        {
            Log.i(LOGTAG, "getEditable");
            return super.getEditable ();
        }

        public ExtractedText getExtractedText (ExtractedTextRequest request, int flags)
        {
            Log.i(LOGTAG, "getExtractedText");
            super.getExtractedText (request, flags);
            return null;
        }

        public CharSequence getSelectedText (int flags)
        {
            Log.i(LOGTAG, "getSelectedText");
            super.getSelectedText (flags);
            return null;
        }

        public CharSequence getTextAfterCursor (int length, int flags)
        {
            Log.i(LOGTAG, "getTextAfterCursor");
            return super.getTextAfterCursor (length, flags);
        }

        public CharSequence getTextBeforeCursor (int length, int flags)
        {
            Log.i(LOGTAG, "getTextBeforeCursor");
            return super.getTextBeforeCursor (length, flags);
        }

        public boolean performContextMenuAction (int id)
        {
            Log.i(LOGTAG, "performContextMenuAction");
            return super.performContextMenuAction (id);
        }

        public boolean performEditorAction (int actionCode)
        {
            Log.i(LOGTAG, "performEditorAction");
            return super.performEditorAction (actionCode);
        }

        public boolean performPrivateCommand (String action, Bundle data)
        {
            Log.i(LOGTAG, "performPrivateCommand");
            return super.performPrivateCommand (action, data);
        }

        public boolean reportFullscreenMode (boolean enabled)
        {
            Log.i(LOGTAG, "reportFullscreenMode");
            return super.reportFullscreenMode (enabled);
        }

        public boolean sendKeyEvent (KeyEvent event)
        {
            Log.i(LOGTAG, "sendKeyEvent");
            return super.sendKeyEvent (event);
        }

        public boolean setComposingRegion (int start, int end)
        {
            Log.i(LOGTAG, "setComposingRegion");
            return super.setComposingRegion (start, end);
        }

        public boolean setComposingText (CharSequence text, int newCursorPosition)
        {
            Log.i(LOGTAG, "setComposingText");
            Log.i(LOGTAG, "composeText: "+text.toString());
            wine_ime_start();
            wine_ime_settext( text.toString(), text.length(), newCursorPosition );
            return super.setComposingText (text, newCursorPosition);
        }

        public boolean setSelection (int start, int end)
        {
            Log.i(LOGTAG, "setSelection");
            return super.setSelection (start, end);
        }
    }

    public native boolean wine_keyboard_event( int hwnd, int action, int keycode, int state );
    public native boolean wine_motion_event( int hwnd, int action, int x, int y, int state, int vscroll );
    public native void wine_surface_changed( int hwnd, Surface surface, boolean is_client );
    public native void wine_desktop_changed( int width, int height );
    public native void wine_config_changed( int dpi );
    private native void wine_run_commandline( String cmdline, String[] wineEnv );
    private native void wine_run_commandarray( String[] cmdarray, String[] wineEnv );
    public native void wine_clear_meta_key_states( int states );
    private native void wine_clipdata_update( int flags, String[] mimetypes );
    public native void wine_set_focus( int hwnd );
    public native void wine_send_syscommand( int hwnd, int param );
    public native void wine_send_window_close( int hwnd );

    //
    // Generic Wine window class
    //

    private HashMap<Integer,WineWindow> win_map = new HashMap<Integer,WineWindow>();
    private ArrayList<WineWindow> win_zorder = new ArrayList<WineWindow>();

    protected class WineWindow extends Object
    {
        static protected final int HWND_TOP = 0;
        static protected final int HWND_BOTTOM = 1;
        static protected final int HWND_TOPMOST = 0xffffffff;
        static protected final int HWND_NOTOPMOST = 0xfffffffe;
        static protected final int HWND_MESSAGE = 0xfffffffd;
        static protected final int SWP_NOZORDER = 0x04;
        static protected final int WS_POPUP = 0x80000000;
        static protected final int WS_CHILD = 0x40000000;
        static protected final int WS_MINIMIZE = 0x20000000;
        static protected final int WS_VISIBLE = 0x10000000;
        static protected final int SC_CLOSE = 0xf060;
        static protected final int SC_MINIMIZE = 0xf020;
        static protected final int SC_RESTORE = 0xf120;

        protected int hwnd;
        protected int owner;
        protected int style;
        protected float scale;
        protected boolean visible;
        protected boolean minimized;
        protected boolean topmost;
        protected boolean use_gl;
        protected Rect visible_rect;
        protected Rect client_rect;
        protected String text;
        protected BitmapDrawable icon;
        protected WineWindow parent;
        protected ArrayList<WineWindow> children;
        protected Surface window_surface;
        protected Surface client_surface;
        protected SurfaceTexture window_surftex;
        protected SurfaceTexture client_surftex;
        protected WineWindowGroup window_group;
        protected WineWindowGroup client_group;

        public WineWindow( int w, WineWindow parent, float scale )
        {
            Log.i( LOGTAG, String.format( "create hwnd %08x parent %08x", w, parent != null ? parent.hwnd : 0 ));
            hwnd = w;
            owner = 0;
            style = 0;
            visible = false;
            minimized = false;
            use_gl = false;
            visible_rect = client_rect = new Rect( 0, 0, 0, 0 );
            this.parent = parent;
            this.scale = scale;
            children = new ArrayList<WineWindow>();
            win_map.put( w, this );
            if (parent != null) parent.children.add( this );
        }

        public void destroy()
        {
            Log.i( LOGTAG, String.format( "destroy hwnd %08x", hwnd ));
            visible = false;
            win_map.remove( this );
            win_zorder.remove( this );
            if (parent != null) parent.children.remove( this );
            destroy_window_groups();
        }

        public WineWindowGroup create_window_groups( Activity act )
        {
            if (client_group != null) return client_group;
            window_group = new WineWindowGroup( this, act );
            client_group = new WineWindowGroup( this, act );
            window_group.addView( client_group );
            client_group.set_layout( client_rect.left - visible_rect.left,
                                     client_rect.top - visible_rect.top,
                                     client_rect.right - visible_rect.left,
                                     client_rect.bottom - visible_rect.top );
            if (parent != null)
            {
                parent.create_window_groups( act );
                if (visible) add_view_to_parent();
                window_group.set_layout( visible_rect.left, visible_rect.top,
                                         visible_rect.right, visible_rect.bottom );
            }
            return client_group;
        }

        public void destroy_window_groups()
        {
            if (window_group != null) window_group.destroy_view();
            if (client_group != null) client_group.destroy_view();
            window_group = null;
            client_group = null;
        }

        public View create_whole_view( Activity act )
        {
            if (window_group == null) create_window_groups( act );
            View view = window_group.create_view( false );
            view.setFocusable( true );
            view.setFocusableInTouchMode( true );
            return window_group;
        }

        public void create_client_view()
        {
            if (client_group == null) return; // no window groups yet
            Log.i( LOGTAG, String.format( "creating client view %08x %s", hwnd, client_rect ));
            View view = client_group.create_view( true );
            client_group.set_layout( 0, 0, 1, 1 ); // make sure the surface gets created
        }

        public void viewAttachedToWindow()
        {
            // Nothing to do
        }

        protected void add_view_to_parent()
        {
            int pos = parent.client_group.getChildCount() - 1;

            // the content view is always last
            if (pos >= 0 && parent.client_group.getChildAt( pos ) == parent.client_group.get_content_view()) pos--;

            for (int i = 0; i < parent.children.size() && pos >= 0; i++)
            {
                WineWindow child = parent.children.get( i );
                if (child == this) break;
                if (!child.visible) continue;
                if (child == ((WineWindowGroup)parent.client_group.getChildAt( pos )).get_window()) pos--;
            }
            parent.client_group.addView( window_group, pos + 1 );

            String str = "";
            for (int i = parent.client_group.getChildCount() - 1; i >= 0; i--)
            {
                View child_view = parent.client_group.getChildAt( i );
                if (child_view == parent.client_group.get_content_view()) str = " (content)" + str;
                else str = String.format( " %08x", ((WineWindowGroup)child_view).get_window().hwnd ) + str;
            }
            Log.i( LOGTAG, String.format( "after adding %08x at %d new views z-order in parent %08x:%s", hwnd, pos + 1, parent.hwnd, str ));
        }

        protected void remove_view_from_parent()
        {
            parent.client_group.removeView( window_group );
        }

        protected void set_zorder( WineWindow prev )
        {
            int pos = -1;

            parent.children.remove( this );
            if (prev != null) pos = parent.children.indexOf( prev );
            parent.children.add( pos + 1, this );

            String str = String.format( "new z-order in parent %08x:", parent.hwnd );
            for (WineWindow child : parent.children.toArray( new WineWindow[0] ))
                str += String.format( child.visible ? " %08x" : " (%08x)", child.hwnd );
            Log.i( LOGTAG, str );
        }

        protected void sync_views_zorder()
        {
            int i, j;

            for (i = parent.children.size() - 1, j = 0; i >= 0; i--)
            {
                WineWindow child = parent.children.get( i );
                if (!child.visible) continue;
                View child_view = parent.client_group.getChildAt( j );
                if (child_view == parent.client_group.get_content_view()) continue;
                if (child != ((WineWindowGroup)child_view).get_window()) break;
                j++;
            }
            while (i >= 0)
            {
                WineWindow child = parent.children.get( i-- );
                if (child.visible && child.window_group != null) child.window_group.bringToFront();
            }

            String str = "";
            for (i = parent.client_group.getChildCount() - 1; i >= 0; i--)
            {
                View child_view = parent.client_group.getChildAt( i );
                if (child_view == parent.client_group.get_content_view()) str = " (content)" + str;
                else str = String.format( " %08x", ((WineWindowGroup)child_view).get_window().hwnd ) + str;
            }
            Log.i( LOGTAG, String.format( "new views z-order in parent %08x:%s", parent.hwnd, str ));
        }

        public void pos_changed( int flags, int insert_after, int owner, int style,
                                 Rect window_rect, Rect client_rect, Rect visible_rect )
        {
            boolean was_visible = visible;
            this.visible_rect = visible_rect;
            this.client_rect = client_rect;
            this.style = style;
            this.owner = owner;
            visible = (style & WS_VISIBLE) != 0;
            minimized = (style & WS_MINIMIZE) != 0;

            Log.i( LOGTAG, String.format( "pos changed hwnd %08x after %08x owner %08x style %08x win %s client %s visible %s flags %08x",
                                          hwnd, insert_after, owner, style, window_rect, client_rect, visible_rect, flags ));

            if ((flags & SWP_NOZORDER) == 0 && parent != null) set_zorder( get_window( insert_after ));

            if (parent != null && window_group != null)
            {
                window_group.set_layout( visible_rect.left, visible_rect.top,
                                         visible_rect.right, visible_rect.bottom );
                if (!was_visible && (style & WS_VISIBLE) != 0) add_view_to_parent();
                else if (was_visible && (style & WS_VISIBLE) == 0) remove_view_from_parent();
                else if (visible && (flags & SWP_NOZORDER) == 0) sync_views_zorder();
            }

            if (client_group != null)
                client_group.set_layout( client_rect.left - visible_rect.left,
                                         client_rect.top - visible_rect.top,
                                         client_rect.right - visible_rect.left,
                                         client_rect.bottom  - visible_rect.top );

            if (parent == null && (flags & SWP_NOZORDER) == 0)
            {
                if (insert_after == HWND_BOTTOM)
                {
                    topmost = false;
                    win_zorder.remove( this );
                    win_zorder.add( this );
                }
                else if (insert_after == HWND_TOP)
                {
                    if (!topmost)
                    {
                        int i;
                        win_zorder.remove( this );
                        for (i = 0; i < win_zorder.size(); i++)
                        {
                            if ( !win_zorder.get( i ).topmost )
                                break;
                        }
                        win_zorder.add( i, this );
                    }
                }
                else if (insert_after == HWND_NOTOPMOST)
                {
                    if (topmost)
                    {
                        int i;
                        topmost = false;
                        win_zorder.remove( this );
                        for (i = 0; i < win_zorder.size(); i++)
                        {
                            if ( !win_zorder.get( i ).topmost )
                                break;
                        }
                        win_zorder.add( i, this );
                    }
                }
                else if (insert_after == HWND_TOPMOST)
                {
                    topmost = true;
                    win_zorder.remove( this );
                    win_zorder.add( 0, this );
                }
                else
                {
                    WineWindow prev_window = get_window( insert_after );
                    win_zorder.remove( this );
                    win_zorder.add( win_zorder.indexOf( prev_window ) + 1, this );
                }

                Log.i( LOGTAG, "z-order:" );
                for (WineWindow win : win_zorder)
                {
                    String attr = "";
                    if (!win.visible)
                        attr = attr + " (hidden)";
                    if (win.minimized)
                        attr = attr + " (minimized)";
                    if (win.topmost)
                        attr = attr + " (topmost)";
                    Log.i( LOGTAG, String.format( "  %08x %s%s", win.hwnd, win.text, attr ) );
                }
            }
        }

        public void set_parent( WineWindow new_parent, float scale )
        {
            Log.i( LOGTAG, String.format( "set parent hwnd %08x parent %08x -> %08x",
                                          hwnd, parent.hwnd, new_parent.hwnd ));
            this.scale = scale;
            if (window_group != null)
            {
                if (visible) remove_view_from_parent();
                new_parent.create_window_groups( window_group.get_activity() );
                window_group.set_layout( visible_rect.left, visible_rect.top,
                                         visible_rect.right, visible_rect.bottom );
            }
            parent.children.remove( this );
            parent = new_parent;
            parent.children.add( this );
            if (visible && window_group != null) add_view_to_parent();
        }

        public void move_children_to_new_parent( WineWindow new_parent, float scale )
        {
            for (WineWindow child : new ArrayList<WineWindow>( children ))
                child.set_parent( new_parent, scale );
        }

        public void focus()
        {
            Log.i( LOGTAG, String.format( "focus hwnd %08x", hwnd ));
        }

        public int get_hwnd()
        {
            return hwnd;
        }

        public int get_owner()
        {
            return owner;
        }

        public int get_style()
        {
            return style;
        }

        public boolean get_use_gl()
        {
            return use_gl;
        }

        public boolean get_topmost()
        {
            return topmost;
        }

        public boolean get_visible()
        {
            return visible;
        }

        public WineWindow get_parent()
        {
            return parent;
        }

        public WineWindow get_top_parent()
        {
            WineWindow win = this;
            while (win.parent != null) win = win.parent;
            return win;
        }

        public void set_text( String str )
        {
            Log.i( LOGTAG, String.format( "set text hwnd %08x '%s'", hwnd, str ));
            text = str;
        }

        public void set_icon( BitmapDrawable bmp )
        {
            Log.i( LOGTAG, String.format( "set icon hwnd %08x", hwnd ));
            icon = bmp;
        }

        public void on_start()
        {
            if (minimized) wine_send_syscommand( hwnd, SC_RESTORE );
            if (window_group != null) window_group.setVisibility( View.VISIBLE );
        }

        public void on_stop()
        {
            if (window_group != null) window_group.setVisibility( View.INVISIBLE );
        }

        private void update_surface( boolean is_client )
        {
            if (is_client)
            {
                Log.i( LOGTAG, String.format( "set client surface hwnd %08x %s", hwnd, client_surface ));
                if (client_surface != null) wine_surface_changed( hwnd, client_surface, true );
            }
            else
            {
                Log.i( LOGTAG, String.format( "set window surface hwnd %08x %s", hwnd, window_surface ));
                if (window_surface != null) wine_surface_changed( hwnd, window_surface, false );
            }
        }

        public void set_surface( SurfaceTexture surftex, boolean is_client )
        {
            if (is_client)
            {
                if (surftex == null) client_surface = null;
                else if (surftex != client_surftex)
                {
                    client_surftex = surftex;
                    client_surface = new Surface( surftex );
                    /* first time shown, set the correct position */
                    client_group.set_layout( client_rect.left - visible_rect.left, client_rect.top - visible_rect.top,
                                             client_rect.right - visible_rect.left, client_rect.bottom  - visible_rect.top );
                }
            }
            else
            {
                if (surftex == null) window_surface = null;
                else if (surftex != window_surftex)
                {
                    window_surftex = surftex;
                    window_surface = new Surface( surftex );
                }
            }
            update_surface( is_client );
        }

        public void start_opengl()
        {
            Log.i( LOGTAG, String.format( "start opengl hwnd %08x", hwnd ));
            use_gl = true;
            create_client_view();
        }

        public void get_event_pos( MotionEvent event, int[] pos )
        {
            pos[0] = Math.round( event.getRawX() / dpi_scale );
            pos[1] = Math.round( event.getRawY() / dpi_scale );
        }
    }

    //
    // Window group for a Wine window, optionally containing a content view
    //

    protected class WineWindowGroup extends ViewGroup
    {
        private WineView content_view;
        private WineWindow win;
        private Activity activity;

        WineWindowGroup( WineWindow win, Activity act )
        {
            super( act );
            activity = act;
            this.win = win;
            setVisibility( View.VISIBLE );
        }

        /* wrapper for layout() making sure that the view is not empty */
        public void set_layout( int left, int top, int right, int bottom )
        {
            if (right <= left + 1) right = left + 2;
            if (bottom <= top + 1) bottom = top + 2;
            layout( left, top, right, bottom );
        }

        @Override
        protected void onLayout( boolean changed, int left, int top, int right, int bottom )
        {
            if (content_view != null) content_view.layout( 0, 0, right - left, bottom - top );
        }

        public WineView create_view( boolean is_client )
        {
            if (content_view != null) return content_view;
            content_view = new WineView( activity, win, is_client );
            addView( content_view );
            return content_view;
        }

        public void destroy_view()
        {
            if (content_view == null) return;
            removeView( content_view );
            content_view = null;
        }

        public WineView get_content_view()
        {
            return content_view;
        }

        public WineWindow get_window()
        {
            return win;
        }

        public Activity get_activity()
        {
            return activity;
        }
    }

    //
    // Wine window implementation using a simple view for each window
    //

    protected class WineWindowView extends WineWindow
    {
        public WineWindowView( Activity activity, int w, float scale )
        {
            super( w, null, scale );
            create_whole_view( activity );
            if (top_view != null) top_view.addView( window_group );
        }

        public void destroy()
        {
            top_view.removeView( window_group );
            super.destroy();
        }

        public void pos_changed( int flags, int insert_after, int new_owner, int style,
                                 Rect window_rect, Rect client_rect, Rect visible_rect )
        {
            boolean show = !visible && (style & WS_VISIBLE) != 0;
            boolean hide = visible && (style & WS_VISIBLE) == 0;

            if (!show && (!visible || hide))
            {
                /* move it off-screen, except that if the texture still needs to be created
                 * we put one pixel on-screen */
                if (window_surface == null)
                    window_group.set_layout( visible_rect.left - visible_rect.right + 1, visible_rect.top - visible_rect.bottom + 1, 1, 1 );
                else
                    window_group.layout( visible_rect.left - visible_rect.right, visible_rect.top - visible_rect.bottom, 0, 0 );
            }
            else window_group.layout( visible_rect.left, visible_rect.top, visible_rect.right, visible_rect.bottom );

            if (show)
            {
                window_group.setFocusable( true );
                window_group.setFocusableInTouchMode( true );
            }
            else if (hide)
            {
                window_group.setFocusable( false );
                window_group.setFocusableInTouchMode( false );
            }

            super.pos_changed( flags, insert_after, new_owner, style, window_rect, client_rect, visible_rect );

            if (show || (flags & SWP_NOZORDER) == 0)
                top_view.update_zorder();

            top_view.update_action_bar();
        }

        public void focus()
        {
            super.focus();
            window_group.setFocusable( true );
            window_group.setFocusableInTouchMode( true );
            updateGamepads();
            top_view.update_action_bar();
        }

        public void set_text( String str )
        {
            super.set_text( str );
            top_view.update_action_bar();
        }

        public void set_icon( BitmapDrawable bmp )
        {
            super.set_icon( bmp );
            top_view.update_action_bar();
        }

        public void set_surface( SurfaceTexture surftex, boolean is_client )
        {
            // move it off-screen if we got a surface while not visible
            if (!is_client && !visible && window_surface == null && surftex != null)
            {
                Log.i(LOGTAG,String.format("hwnd %08x not visible, moving offscreen", hwnd));
                window_group.layout( visible_rect.left - visible_rect.right, visible_rect.top - visible_rect.bottom, 0, 0 );
            }
            super.set_surface( surftex, is_client );
        }

        public void get_event_pos( MotionEvent event, int[] pos )
        {
            pos[0] = Math.round( event.getX() + window_group.getLeft() );
            pos[1] = Math.round( event.getY() + window_group.getTop() );
        }
    }

    // View used for all Wine windows, backed by a TextureView

    protected class WineView extends TextureView implements TextureView.SurfaceTextureListener
    {
        private WineWindow window;
        private boolean is_client;

        private boolean scroll_active;
        private int scroll_device;
        private int scroll_origin_x, scroll_origin_y;
        private int scroll_last_x, scroll_last_y;

        public WineView( Context c, WineWindow win, boolean client )
        {
            super( c );
            window = win;
            is_client = client;
            setSurfaceTextureListener( this );
            setVisibility( VISIBLE );
            setOpaque( false );
        }

        public void onAttachedToWindow()
        {
            Log.i( LOGTAG, String.format( "view %08x attached to window", window.hwnd ));
            if (!is_client) window.viewAttachedToWindow();
        }

        public void onVisibilityChanged( View changed, int visibility )
        {
            Log.i( LOGTAG, String.format( "%08x visibility changed %d", window.hwnd, visibility ));
            if (visibility == View.VISIBLE) window.update_surface( is_client );
        }

        public WineView( AttributeSet attrs )
        {
            super( getActivity(), attrs );
        }

        private float getCenteredAxis(MotionEvent event, InputDevice device, int axis)
        {
            final InputDevice.MotionRange range =
                device.getMotionRange(axis, event.getSource());

            if (range != null)
            {
                final float flat = range.getFlat();
                final float value = event.getAxisValue(axis);

                if (Math.abs(value) > flat)
                {
                    return value;
                }
            }
            return 0;
        }

        public WineWindow get_window()
        {
            return window;
        }

        public void onSurfaceTextureAvailable( SurfaceTexture surftex, int width, int height )
        {
            Log.i( LOGTAG, String.format( "onSurfaceTextureAvailable win %08x %dx%d %s",
                                          window.hwnd, width, height, is_client ? "client" : "whole" ));
            window.set_surface( surftex, is_client );
        }

        public void onSurfaceTextureSizeChanged( SurfaceTexture surftex, int width, int height )
        {
            Log.i( LOGTAG, String.format( "onSurfaceTextureSizeChanged win %08x %dx%d %s",
                                          window.hwnd, width, height, is_client ? "client" : "whole" ));
            window.set_surface( surftex, is_client);
        }

        public boolean onSurfaceTextureDestroyed( SurfaceTexture surftex )
        {
            Log.i( LOGTAG, String.format( "onSurfaceTextureDestroyed win %08x %s",
                                          window.hwnd, is_client ? "client" : "whole" ));
            window.set_surface( null, is_client );
            return false;  // hold on to the texture since the app may still be using it
        }

        public void onSurfaceTextureUpdated(SurfaceTexture surftex)
        {
        }

        @TargetApi(24)
        public PointerIcon onResolvePointerIcon( MotionEvent event, int index )
        {
            return current_cursor;
        }

        public boolean onGenericMotionEvent( MotionEvent event )
        {
            if (is_client || window.parent != null) return false;
            Log.i(LOGTAG, "view generic motion event");
            if ((event.getSource() & InputDevice.SOURCE_JOYSTICK) ==
                InputDevice.SOURCE_JOYSTICK &&
                event.getAction() == MotionEvent.ACTION_MOVE)
                {
                    Log.i(LOGTAG, "Joystick Motion");
                    InputDevice mDevice = event.getDevice();
                    float[] axis;

                    axis = new float[10];
                    axis[0] = getCenteredAxis(event, mDevice,  MotionEvent.AXIS_X);
                    axis[1] = getCenteredAxis(event, mDevice,  MotionEvent.AXIS_Y);
                    axis[2] = getCenteredAxis(event, mDevice,  MotionEvent.AXIS_Z);
                    axis[3] = getCenteredAxis(event, mDevice,  MotionEvent.AXIS_RX);
                    axis[4] = getCenteredAxis(event, mDevice,  MotionEvent.AXIS_RY);
                    axis[5] = getCenteredAxis(event, mDevice,  MotionEvent.AXIS_RZ);
                    axis[6] = getCenteredAxis(event, mDevice,  MotionEvent.AXIS_LTRIGGER);
                    if (axis[6] == 0)
                        axis[6] = getCenteredAxis(event, mDevice,  MotionEvent.AXIS_BRAKE);
                    axis[7] = getCenteredAxis(event, mDevice,  MotionEvent.AXIS_RTRIGGER);
                    if (axis[7] == 0)
                        axis[7] = getCenteredAxis(event, mDevice,  MotionEvent.AXIS_GAS);
                    axis[8] = getCenteredAxis(event, mDevice,  MotionEvent.AXIS_HAT_X);
                    axis[9] = getCenteredAxis(event, mDevice,  MotionEvent.AXIS_HAT_Y);

                    wine_send_gamepad_axis(event.getDeviceId(), axis);
                    return true;
                }
            if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0 &&
                event.getAction() == MotionEvent.ACTION_SCROLL)
            {
                int[] pos = new int[2];
                int hscroll = (int)event.getAxisValue(MotionEvent.AXIS_HSCROLL);
                int vscroll = (int)event.getAxisValue(MotionEvent.AXIS_VSCROLL);
                window.get_event_pos( event, pos );
                Log.i(LOGTAG, String.format( "view scroll event win %08x action %d pos %d,%d buttons %04x view %d,%d scroll %d,%d",
                                             window.hwnd, event.getAction(), pos[0], pos[1], event.getButtonState(), getLeft(), getTop(), hscroll, vscroll ));

                if (vscroll != 0)
                    wine_motion_event( window.hwnd, event.getAction(), pos[0], pos[1], event.getButtonState(),
                                       vscroll < 0 ? -120 : 120 );
                if (hscroll != 0)
                    wine_motion_event( window.hwnd, event.getAction() | 0x10000, pos[0], pos[1], event.getButtonState(),
                                       hscroll < 0 ? 120 : -120 );

                return true;
            }
            if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0)
            {
                int[] pos = new int[2];
                window.get_event_pos( event, pos );
                Log.i( LOGTAG, String.format( "view motion event win %08x action %d pos %d,%d buttons %04x view %d,%d",
                                              window.hwnd, event.getAction(), pos[0], pos[1],
                                              event.getButtonState(), getLeft(), getTop() ));
                return wine_motion_event( window.hwnd, event.getAction(), pos[0], pos[1],
                                          event.getButtonState(), 0 );
            }
            return super.onGenericMotionEvent(event);
        }

        public boolean onKeyDown(int keyCode, KeyEvent event)
        {
            Log.i(LOGTAG, "Keydown");
            if ((event.getSource() & InputDevice.SOURCE_GAMEPAD)
                == InputDevice.SOURCE_GAMEPAD)
            {
                    Log.i(LOGTAG, "Is Gamepad "+keyCode);
                    wine_send_gamepad_button(event.getDeviceId(), keyCode, 0xff);
                    return true;
            }
            return super.onKeyDown(keyCode, event);
        }

        public boolean onKeyUp(int keyCode, KeyEvent event)
        {
            Log.i(LOGTAG, "KeyUp");
            if ((event.getSource() & InputDevice.SOURCE_GAMEPAD)
                == InputDevice.SOURCE_GAMEPAD)
            {
                    Log.i(LOGTAG, "Is Gamepad: "+keyCode);
                    wine_send_gamepad_button(event.getDeviceId(), keyCode, 0x0);
                    return true;
            }
            return super.onKeyDown(keyCode, event);
        }

        public boolean onTouchEvent( MotionEvent event )
        {
            if (is_client || window.parent != null) return false;
            int[] pos = new int[2];
            window.get_event_pos( event, pos );
            Log.i( LOGTAG, String.format( "view touch event win %08x action %d pos %d,%d buttons %04x view %d,%d",
                                          window.hwnd, event.getAction(), pos[0], pos[1],
                                          event.getButtonState(), getLeft(), getTop() ));
            if ((event.getSource() & InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE &&
                event.getButtonState() == 0 && event.getActionMasked() == MotionEvent.ACTION_DOWN)
            {
                Log.i( LOGTAG, "view begin touchpad scroll" );

                scroll_active = true;
                scroll_device = event.getDeviceId();
                scroll_origin_x = pos[0];
                scroll_origin_y = pos[1];
                scroll_last_x = pos[0];
                scroll_last_y = pos[1];

                /* Ensure the mouse is in position, but don't press a button */
                return wine_motion_event( window.hwnd, MotionEvent.ACTION_HOVER_MOVE, pos[0], pos[1], event.getButtonState(), 0 );
            }
            if (scroll_active && event.getDeviceId() == scroll_device)
            {
                if (event.getActionMasked() == MotionEvent.ACTION_MOVE)
                {
                    int hscroll = scroll_last_x - pos[0];
                    int vscroll = pos[1] - scroll_last_y;

                    scroll_last_x = pos[0];
                    scroll_last_y = pos[1];

                    if (hscroll != 0)
                    {
                        Log.i( LOGTAG, String.format( "view touchpad hscroll %d", hscroll ) );
                        wine_motion_event( window.hwnd, MotionEvent.ACTION_SCROLL | 0x10000, scroll_origin_x, scroll_origin_y, event.getButtonState(), hscroll );
                    }
                    if (vscroll != 0)
                    {
                        Log.i( LOGTAG, String.format( "view touchpad vscroll %d", vscroll ) );
                        wine_motion_event( window.hwnd, MotionEvent.ACTION_SCROLL, scroll_origin_x, scroll_origin_y, event.getButtonState(), vscroll );
                    }
                    return true;
                }
                else if (event.getActionMasked() == MotionEvent.ACTION_UP)
                {
                    Log.i( LOGTAG, "view end touchpad scroll" );
                    scroll_active = false;
                    /* There was no button down, so prevent the button up */
                    return wine_motion_event( window.hwnd, MotionEvent.ACTION_HOVER_MOVE, scroll_origin_x, scroll_origin_y, event.getButtonState(), 0 );
                }
                else if (event.getActionMasked() == MotionEvent.ACTION_CANCEL)
                {
                    Log.i( LOGTAG, "view cancel touchpad scroll" );
                    int hscroll = scroll_last_x - scroll_origin_x;
                    int vscroll = scroll_origin_y - scroll_last_y;
                    scroll_active = false;
                    if (hscroll != 0)
                    {
                        Log.i( LOGTAG, String.format( "view touchpad hscroll %d", hscroll ) );
                        wine_motion_event( window.hwnd, MotionEvent.ACTION_SCROLL | 0x10000, scroll_origin_x, scroll_origin_y, event.getButtonState(), hscroll );
                    }
                    if (vscroll != 0)
                    {
                        Log.i( LOGTAG, String.format( "view touchpad vscroll %d", vscroll ) );
                        wine_motion_event( window.hwnd, MotionEvent.ACTION_SCROLL, scroll_origin_x, scroll_origin_y, event.getButtonState(), vscroll );
                    }
                    return wine_motion_event( window.hwnd, MotionEvent.ACTION_HOVER_MOVE, scroll_origin_x, scroll_origin_y, event.getButtonState(), 0 );
                }
            }
            return wine_motion_event( window.hwnd, event.getAction(), pos[0], pos[1],
                                      event.getButtonState(), 0 );
        }

        public boolean dispatchKeyEvent( KeyEvent event )
        {
            Log.i( LOGTAG, String.format( "view key event win %08x action %d keycode %d (%s)",
                                          window.hwnd, event.getAction(), event.getKeyCode(),
                                          KeyEvent.keyCodeToString( event.getKeyCode() )));;
            boolean ret = wine_keyboard_event( window.hwnd, event.getAction(), event.getKeyCode(),
                                               event.getMetaState() );
            if (!ret) ret = super.dispatchKeyEvent(event);
            return ret;
        }

        public boolean onCheckIsTextEditor()
        {
            Log.i(LOGTAG, "onCheckIsTextEditor");
            return true;
        }

        public InputConnection onCreateInputConnection( EditorInfo outAttrs )
        {
            Log.i(LOGTAG, "onCreateInputConnection");
            outAttrs.inputType = InputType.TYPE_NULL;
            outAttrs.imeOptions = EditorInfo.IME_NULL;
            /* Disable voice for now. It double inputs until we can
               Support deletion of text in the document */
            outAttrs.privateImeOptions = "nm";
            return new WineInputConnection( this );
        }
    }

    protected class TopView extends ViewGroup
    {
        protected WineWindow desktop_window;
        protected WineView desktop_view;

        public TopView( int hwnd )
        {
            super( context );
            desktop_window = new WineWindow( hwnd, null, 1.0f );
            desktop_view = new WineView( context, desktop_window, false );
            desktop_window.visible = true;
            addView( desktop_view );
        }

        @Override
        protected void onSizeChanged( int width, int height, int old_width, int old_height )
        {
            Log.i(LOGTAG, "desktop size " + width + "x" + height );
            desktop_view.layout( 0, 0, width, height );
            wine_desktop_changed( (int)(width / dpi_scale), (int)(height / dpi_scale) );
        }

        @Override
        protected void onLayout( boolean changed, int left, int top, int right, int bottom )
        {
            // nothing to do
        }

        @Override
        public boolean dispatchKeyEvent( KeyEvent event )
        {
            return desktop_view.dispatchKeyEvent( event );
        }

        private void update_zorder()
        {
            int i, j;

            i = win_zorder.size() - 1;
            j = 0;

            while (i > -1 && j < getChildCount())
            {
                if (!win_zorder.get( i ).visible || win_zorder.get( i ).minimized)
                {
                    i--;
                }
                else if (!(getChildAt( j ) instanceof WineWindowGroup))
                {
                    j++;
                }
                else
                {
                    WineWindow win = ((WineWindowGroup)getChildAt( j )).get_window();
                    if (win == win_zorder.get( i ))
                        i--;
                    j++;
                }
            }

            while (i > -1)
            {
                if (win_zorder.get( i ) instanceof WineWindowView)
                {
                    WineWindow win = win_zorder.get( i );
                    if (win.visible && !win.minimized) win.window_group.bringToFront();
                }
                i--;
            }
        }

        private void update_action_bar()
        {
            for (int i = getChildCount() - 1; i >= 0; i--)
            {
                View v = getChildAt( i );
                if (v instanceof WineWindowGroup)
                {
                    WineWindow win = ((WineWindowGroup)v).get_window();
                    Log.i( LOGTAG, String.format( "%d: %08x %s", i, win.hwnd, win.text ));
                    if (win.owner != 0) continue;
                    if (!win.visible) continue;
                    if (win.text == null) continue;
                    getActivity().setTitle( win.text );
                    if (getActivity().getActionBar() != null)
                    {
                        if (win.icon != null)
                            getActivity().getActionBar().setIcon( win.icon );
                        else
                            getActivity().getActionBar().setIcon( R.drawable.wine );
                    }
                    return;
                }
            }
            getActivity().setTitle( R.string.org_winehq_wine_app_name);
            if (getActivity().getActionBar() != null)
                getActivity().getActionBar().setIcon( R.drawable.wine );
        }
    }

    protected static TopView top_view;

    protected WineWindow get_window( int hwnd )
    {
        return win_map.get( hwnd );
    }

    protected WineWindow[] get_zorder()
    {
        return win_zorder.toArray( new WineWindow[0] );
    }

    private void createYDrive()
    {
        String[] cmd = new String[4];
        cmd[0] = "/system/bin/ln";
        cmd[1] = "-sf";
        cmd[2] = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).toString();
        cmd[3] = prefix.getAbsolutePath() + "/dosdevices/y:";
        try
        {
            java.lang.Process p = Runtime.getRuntime().exec( cmd );
            p.waitFor();
        }
        catch( Exception e )
        {
            Log.i( LOGTAG, "Exception creating y: drive for prefix " + prefix.getAbsolutePath() + " : " + e );
        }
    }

    public void create_desktop_window( int hwnd, int scaling )
    {
        Log.i( LOGTAG, String.format( "create desktop %08x scale %d", hwnd, scaling ));
        create_window( WineWindow.HWND_MESSAGE, false, 0, 1.0f, 0, null );
        top_view = new TopView( hwnd );
        if (startupDpi == 0)
            startupDpi = context.getResources().getConfiguration().densityDpi;
        if (scaling == 0)  /* no scaling */
        {
            dpi_scale = 1.0f;
        }
        else if (scaling == 1)  /* scale to screen DPI */
        {
            dpi_scale = startupDpi / 96.0f;
            startupDpi = 96;
        }
        else
        {
            dpi_scale = scaling / 96.0f;
            startupDpi = 96;
        }
        wine_config_changed( startupDpi );
        synchronized( desktopReadyLock )
        {
            desktopReady = true;
            if (firstRun) createYDrive();

            for (WineCmdLine cmdline : queuedCmdlines)
                runCmdline( cmdline.cmdline, cmdline.env );
            queuedCmdlines = null;
            for (WineCmdArray cmdarray : queuedCmdarrays)
                runCmdarray( cmdarray.cmdarray, cmdarray.env );
            queuedCmdarrays = null;
        }
    }

    public void create_window( int hwnd, boolean opengl, int parent, float scale, int pid, String wingroup )
    {
        WineWindow win = get_window( hwnd );
        if (win == null)
        {
            if (parent != 0 || hwnd == WineWindow.HWND_MESSAGE)
            {
                win = new WineWindow( hwnd, get_window( parent ), scale );
                win.create_window_groups( activity );
            }
            else
            {
                win = new WineWindowView( activity, hwnd, scale );
            }
        }
        else if (opengl) win.start_opengl();
    }

    public void destroy_window( int hwnd )
    {
        WineWindow win = get_window( hwnd );
        if (win != null) win.destroy();
    }

    public void close_window( int hwnd )
    {
        if (hwnd != 0) wine_send_window_close( hwnd );
    }

    public void set_window_parent( int hwnd, int parent, float scale, int pid )
    {
        WineWindow win = get_window( hwnd );
        if (win == null) return;
        if (win.parent == null)  // top-level -> child
        {
            WineWindow new_win = new WineWindow( hwnd, get_window( parent ), scale );
            new_win.create_window_groups( activity );
            win.move_children_to_new_parent( new_win, scale );
            win.destroy();
        }
        else if (parent == 0)  // child -> top_level
        {
            WineWindow new_win = new WineWindowView( activity, hwnd, scale );
            win.move_children_to_new_parent( new_win, scale );
            win.destroy();
        }
        else win.set_parent( get_window( parent ), scale );
    }

    public void focus_window( int hwnd )
    {
        WineWindow win = get_window( hwnd );
        if (win != null) win.focus();
    }

    public void set_window_icon( int hwnd, int width, int height, int icon[] )
    {
        WineWindow win = get_window( hwnd );
        if (win == null) return;
        BitmapDrawable new_icon = null;
        if (icon != null) new_icon = new BitmapDrawable( context.getResources(),
                                                         Bitmap.createBitmap( icon, width, height, Bitmap.Config.ARGB_8888 ) );
        win.set_icon( new_icon );
    }

    @TargetApi(24)
    public void set_cursor( int id, int width, int height, int hotspotx, int hotspoty, int bits[] )
    {
        Log.i( LOGTAG, String.format( "set_cursor id %d size %dx%d hotspot %dx%d", id, width, height, hotspotx, hotspoty ));
        if (bits != null)
        {
            Bitmap bitmap = Bitmap.createBitmap( bits, width, height, Bitmap.Config.ARGB_8888 );
            current_cursor = PointerIcon.create( bitmap, hotspotx, hotspoty );
        }
        else current_cursor = PointerIcon.getSystemIcon( context, id );
    }

    public void set_window_text( int hwnd, String text )
    {
        WineWindow win = get_window( hwnd );
        if (win != null) win.set_text( text );
    }

    public void window_pos_changed( int hwnd, int flags, int insert_after, int owner, int style,
                                    Rect window_rect, Rect client_rect, Rect visible_rect )
    {
        WineWindow win = get_window( hwnd );
        if (win != null)
            win.pos_changed( flags, insert_after, owner, style, window_rect, client_rect, visible_rect );
    }

    private Uri get_clipboard_uri()
    {
        String authority = providerAuthority;
        if (authority == null)
            return null;
        else
            return Uri.parse("content://" + authority + "/copying");
    }

    public void poll_clipdata()
    {
        if (clipboard_manager == null)
        {
            clipboard_manager = (ClipboardManager)context.getSystemService( Activity.CLIPBOARD_SERVICE );
            clipboard_manager.addPrimaryClipChangedListener( this );
        }

        clipdata = clipboard_manager.getPrimaryClip();

        int flags=0;
        String[] mimetypes = null;

        if (clipdata != null && clipdata.getItemCount() != 0)
        {
            ClipData.Item item = clipdata.getItemAt( 0 );

            Uri uri = item.getUri();

            if (uri != null)
            {
                flags |= CLIPDATA_HASURI;
                if (uri.equals(get_clipboard_uri()))
                    flags |= CLIPDATA_OURS;
            }

            CharSequence text = item.getText();

            if (text != null)
            {
                flags |= CLIPDATA_HASTEXT;
                if (text.length() == 0)
                    flags |= CLIPDATA_EMPTYTEXT;
                if (text instanceof Spanned)
                    flags |= CLIPDATA_SPANNEDTEXT;
            }

            String htmlText = item.getHtmlText();
            
            if (htmlText != null)
            {
                flags |= CLIPDATA_HASHTML;
            }

            Intent intent = item.getIntent();

            if (intent != null)
            {
                flags |= CLIPDATA_HASINTENT;
            }

            ClipDescription clipdesc = clipdata.getDescription();

            if (clipdesc != null)
                mimetypes = clipdesc.filterMimeTypes( "*/*" );
        }

        wine_clipdata_update( flags, mimetypes );
    }

    public void onPrimaryClipChanged()
    {
        runOnUiThread( new Runnable() { public void run() { poll_clipdata(); }} );
    }

    public String getClipdata( int flags )
    {
        /* This runs on the device thread! Be careful of potential races or blocking. */
        ClipData clipdata = this.clipdata;

        if (clipdata == null)
            return "";

        if (clipdata.getItemCount() == 0)
            return "";

        ClipData.Item item = clipdata.getItemAt(0);

        if (flags == CLIPDATA_HASTEXT)
        {
            CharSequence text = item.getText();
            if (text != null)
                return text.toString();
        }

        return "";
    }

    public void set_clipdata( String text )
    {
        if (clipboard_manager == null)
        {
            clipboard_manager = (ClipboardManager)context.getSystemService( Activity.CLIPBOARD_SERVICE );
            clipboard_manager.addPrimaryClipChangedListener( this );
        }

        ClipData.Item item = new ClipData.Item( text, null, get_clipboard_uri() );

        ClipData data = new ClipData( "Wine clipboard", new String[0], item );

        clipboard_manager.setPrimaryClip( data );
    }

    public void createDesktopWindow( final int hwnd, final int scaling )
    {
        runOnUiThread( new Runnable() { public void run() { create_desktop_window( hwnd, scaling ); }} );
    }

    public void createWindow( final int hwnd, final boolean opengl, final int parent, final float scale, final int pid, final String wingroup )
    {
        runOnUiThread( new Runnable() { public void run() { create_window( hwnd, opengl, parent, scale, pid, wingroup ); }} );
    }

    public void destroyWindow( final int hwnd )
    {
        runOnUiThread( new Runnable() { public void run() { destroy_window( hwnd ); }} );
    }

    public void sendWindowClose( final int hwnd )
    {
        runOnUiThread( new Runnable() { public void run() { close_window( hwnd ); }} );
    }

    public void setParent( final int hwnd, final int parent, final float scale, final int pid )
    {
        runOnUiThread( new Runnable() { public void run() { set_window_parent( hwnd, parent, scale, pid ); }} );
    }

    public void setFocus( final int hwnd )
    {
        runOnUiThread( new Runnable() { public void run() { focus_window( hwnd ); }} );
    }

    public void setWindowIcon( final int hwnd, final int width, final int height, final int icon[] )
    {
        runOnUiThread( new Runnable() { public void run() { set_window_icon( hwnd, width, height, icon ); }} );
    }

    public void setWindowText( final int hwnd, final String text )
    {
        runOnUiThread( new Runnable() { public void run() { set_window_text( hwnd, text ); }} );
    }

    public void setCursor( final int id, final int width, final int height,
                           final int hotspotx, final int hotspoty, final int bits[] )
    {
        if (Build.VERSION.SDK_INT < 24) return;
        runOnUiThread( new Runnable() { public void run() { set_cursor( id, width, height, hotspotx, hotspoty, bits ); }} );
    }

    public void windowPosChanged( final int hwnd, final int flags, final int insert_after,
                                  final int owner, final int style,
                                  final int window_left, final int window_top,
                                  final int window_right, final int window_bottom,
                                  final int client_left, final int client_top,
                                  final int client_right, final int client_bottom,
                                  final int visible_left, final int visible_top,
                                  final int visible_right, final int visible_bottom )
    {
        final Rect window_rect = new Rect( window_left, window_top, window_right, window_bottom );
        final Rect client_rect = new Rect( client_left, client_top, client_right, client_bottom );
        final Rect visible_rect = new Rect( visible_left, visible_top, visible_right, visible_bottom );
        runOnUiThread( new Runnable() {
            public void run() { window_pos_changed( hwnd, flags, insert_after, owner, style,
                                                    window_rect, client_rect, visible_rect ); }} );
    }

    public void pollClipdata()
    {
        runOnUiThread( new Runnable() { public void run() { poll_clipdata(); }} );
    }
    
    public void setClipdata( final String text )
    {
        runOnUiThread( new Runnable() { public void run() { set_clipdata(text); }} );
    }
}
