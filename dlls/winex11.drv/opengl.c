/*
 * X11DRV OpenGL functions
 *
 * Copyright 2000 Lionel Ulmer
 * Copyright 2005 Alex Woods
 * Copyright 2005 Raphael Junqueira
 * Copyright 2006-2009 Roderick Colenbrander
 * Copyright 2006 Tomas Carnecky
 * Copyright 2012 Alexandre Julliard
 * Copyright 2013 Matteo Bruni
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#include "x11drv.h"
#include "xcomposite.h"
#include "winternl.h"
#include "wine/library.h"
#include "wine/debug.h"

#ifdef SONAME_LIBGL

WINE_DEFAULT_DEBUG_CHANNEL(wgl);
WINE_DECLARE_DEBUG_CHANNEL(winediag);

#include "wine/wgl.h"
#include "wine/wgl_driver.h"

#ifdef HAVE_EGL_EGL_H
#ifdef HAVE_EGL_EGLEXT_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_KHR_create_context_no_error
#define EGL_KHR_create_context_no_error 1
#define EGL_CONTEXT_OPENGL_NO_ERROR_KHR 0x31b3
#endif

#endif
#endif

typedef struct __GLXcontextRec *GLXContext;
typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef XID GLXPixmap;
typedef XID GLXDrawable;
typedef XID GLXFBConfigID;
typedef XID GLXContextID;
typedef XID GLXWindow;
typedef XID GLXPbuffer;

#define GLX_USE_GL                        1
#define GLX_BUFFER_SIZE                   2
#define GLX_LEVEL                         3
#define GLX_RGBA                          4
#define GLX_DOUBLEBUFFER                  5
#define GLX_STEREO                        6
#define GLX_AUX_BUFFERS                   7
#define GLX_RED_SIZE                      8
#define GLX_GREEN_SIZE                    9
#define GLX_BLUE_SIZE                     10
#define GLX_ALPHA_SIZE                    11
#define GLX_DEPTH_SIZE                    12
#define GLX_STENCIL_SIZE                  13
#define GLX_ACCUM_RED_SIZE                14
#define GLX_ACCUM_GREEN_SIZE              15
#define GLX_ACCUM_BLUE_SIZE               16
#define GLX_ACCUM_ALPHA_SIZE              17

#define GLX_BAD_SCREEN                    1
#define GLX_BAD_ATTRIBUTE                 2
#define GLX_NO_EXTENSION                  3
#define GLX_BAD_VISUAL                    4
#define GLX_BAD_CONTEXT                   5
#define GLX_BAD_VALUE                     6
#define GLX_BAD_ENUM                      7

#define GLX_VENDOR                        1
#define GLX_VERSION                       2
#define GLX_EXTENSIONS                    3

#define GLX_CONFIG_CAVEAT                 0x20
#define GLX_DONT_CARE                     0xFFFFFFFF
#define GLX_X_VISUAL_TYPE                 0x22
#define GLX_TRANSPARENT_TYPE              0x23
#define GLX_TRANSPARENT_INDEX_VALUE       0x24
#define GLX_TRANSPARENT_RED_VALUE         0x25
#define GLX_TRANSPARENT_GREEN_VALUE       0x26
#define GLX_TRANSPARENT_BLUE_VALUE        0x27
#define GLX_TRANSPARENT_ALPHA_VALUE       0x28
#define GLX_WINDOW_BIT                    0x00000001
#define GLX_PIXMAP_BIT                    0x00000002
#define GLX_PBUFFER_BIT                   0x00000004
#define GLX_AUX_BUFFERS_BIT               0x00000010
#define GLX_FRONT_LEFT_BUFFER_BIT         0x00000001
#define GLX_FRONT_RIGHT_BUFFER_BIT        0x00000002
#define GLX_BACK_LEFT_BUFFER_BIT          0x00000004
#define GLX_BACK_RIGHT_BUFFER_BIT         0x00000008
#define GLX_DEPTH_BUFFER_BIT              0x00000020
#define GLX_STENCIL_BUFFER_BIT            0x00000040
#define GLX_ACCUM_BUFFER_BIT              0x00000080
#define GLX_NONE                          0x8000
#define GLX_SLOW_CONFIG                   0x8001
#define GLX_TRUE_COLOR                    0x8002
#define GLX_DIRECT_COLOR                  0x8003
#define GLX_PSEUDO_COLOR                  0x8004
#define GLX_STATIC_COLOR                  0x8005
#define GLX_GRAY_SCALE                    0x8006
#define GLX_STATIC_GRAY                   0x8007
#define GLX_TRANSPARENT_RGB               0x8008
#define GLX_TRANSPARENT_INDEX             0x8009
#define GLX_VISUAL_ID                     0x800B
#define GLX_SCREEN                        0x800C
#define GLX_NON_CONFORMANT_CONFIG         0x800D
#define GLX_DRAWABLE_TYPE                 0x8010
#define GLX_RENDER_TYPE                   0x8011
#define GLX_X_RENDERABLE                  0x8012
#define GLX_FBCONFIG_ID                   0x8013
#define GLX_RGBA_TYPE                     0x8014
#define GLX_COLOR_INDEX_TYPE              0x8015
#define GLX_MAX_PBUFFER_WIDTH             0x8016
#define GLX_MAX_PBUFFER_HEIGHT            0x8017
#define GLX_MAX_PBUFFER_PIXELS            0x8018
#define GLX_PRESERVED_CONTENTS            0x801B
#define GLX_LARGEST_PBUFFER               0x801C
#define GLX_WIDTH                         0x801D
#define GLX_HEIGHT                        0x801E
#define GLX_EVENT_MASK                    0x801F
#define GLX_DAMAGED                       0x8020
#define GLX_SAVED                         0x8021
#define GLX_WINDOW                        0x8022
#define GLX_PBUFFER                       0x8023
#define GLX_PBUFFER_HEIGHT                0x8040
#define GLX_PBUFFER_WIDTH                 0x8041
#define GLX_SWAP_METHOD_OML               0x8060
#define GLX_SWAP_EXCHANGE_OML             0x8061
#define GLX_SWAP_COPY_OML                 0x8062
#define GLX_SWAP_UNDEFINED_OML            0x8063
#define GLX_RGBA_BIT                      0x00000001
#define GLX_COLOR_INDEX_BIT               0x00000002
#define GLX_PBUFFER_CLOBBER_MASK          0x08000000

/** GLX_ARB_multisample */
#define GLX_SAMPLE_BUFFERS_ARB            100000
#define GLX_SAMPLES_ARB                   100001
/** GLX_ARB_framebuffer_sRGB */
#define GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT  0x20B2
/** GLX_EXT_fbconfig_packed_float */
#define GLX_RGBA_UNSIGNED_FLOAT_TYPE_EXT  0x20B1
#define GLX_RGBA_UNSIGNED_FLOAT_BIT_EXT   0x00000008
/** GLX_ARB_create_context */
#define GLX_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB     0x2092
#define GLX_CONTEXT_FLAGS_ARB             0x2094
/** GLX_ARB_create_context_no_error */
#define GLX_CONTEXT_OPENGL_NO_ERROR_ARB   0x31B3
/** GLX_ARB_create_context_profile */
#define GLX_CONTEXT_PROFILE_MASK_ARB      0x9126
/** GLX_ATI_pixel_format_float */
#define GLX_RGBA_FLOAT_ATI_BIT            0x00000100
/** GLX_ARB_pixel_format_float */
#define GLX_RGBA_FLOAT_BIT                0x00000004
#define GLX_RGBA_FLOAT_TYPE               0x20B9
/** GLX_MESA_query_renderer */
#define GLX_RENDERER_ID_MESA              0x818E
/** GLX_NV_float_buffer */
#define GLX_FLOAT_COMPONENTS_NV           0x20B0


static char *glExtensions;
static const char *glxExtensions;
#ifdef SONAME_LIBEGL
static BOOL supports_khr_create_context;
#endif
static char wglExtensions[4096];
static int glxVersion[2];

struct wgl_pixel_format
{
    union
    {
        GLXFBConfig fbc;
#ifdef SONAME_LIBEGL
        EGLConfig   c;
#endif
    } config;
    XVisualInfo *visual;
    int         fmt_id;
    int         render_type;
    DWORD       dwFlags; /* We store some PFD_* flags in here for emulated bitmap formats */
};

struct wgl_context
{
    HDC hdc;
    BOOL has_been_current;
    BOOL sharing;
    BOOL gl3_context;
    const struct wgl_pixel_format *fmt;
    int numAttribs; /* This is needed for delaying wglCreateContextAttribsARB */
    int attribList[16]; /* This is needed for delaying wglCreateContextAttribsARB */
    BOOL gles; /* This flag is only used in EGL, GLX uses the context attribs for that */
    union
    {
        GLXContext glx;
#ifdef SONAME_LIBEGL
        EGLContext egl;
#endif
    } ctx;
    struct gl_drawable *drawables[2];
    struct gl_drawable *new_drawables[2];
    BOOL refresh_drawables;
    struct list entry;
};

struct wgl_pbuffer
{
    union
    {
        Drawable   drawable;
#ifdef SONAME_LIBEGL
        EGLSurface surface;
#endif
    } pbuffer;
    const struct wgl_pixel_format* fmt;
    int        width;
    int        height;
    int*       attribList;
    int        use_render_texture; /* This is also the internal texture format */
    int        texture_bind_target;
    int        texture_bpp;
    GLint      texture_format;
    GLuint     texture_target;
    GLenum     texture_type;
    GLuint     texture;
    int        texture_level;
    GLXContext tmp_context;
    GLXContext prev_context;
    struct list entry;
};

enum dc_gl_type
{
    DC_GL_NONE,       /* no GL support (pixel format not set yet) */
    DC_GL_WINDOW,     /* normal top-level window */
    DC_GL_CHILD_WIN,  /* child window using XComposite */
    DC_GL_PIXMAP_WIN, /* child window using intermediate pixmap */
    DC_GL_PBUFFER     /* pseudo memory DC using a PBuffer */
};

struct gl_drawable
{
    LONG                           ref;          /* reference count */
    enum dc_gl_type                type;         /* type of GL surface */
    struct
    {
        GLXDrawable d;
#ifdef SONAME_LIBEGL
        EGLSurface s;
#endif
    } drawable; /* drawable for rendering to the client area */
    Window                         window;       /* window if drawable is a GLXWindow */
    Pixmap                         pixmap;       /* base pixmap if drawable is a GLXPixmap */
    const struct wgl_pixel_format *format;       /* pixel format for the drawable */
    SIZE                           pixmap_size;  /* pixmap size for GLXPixmap drawables */
    int                            swap_interval;
    BOOL                           refresh_swap_interval;
};

enum glx_swap_control_method
{
    GLX_SWAP_CONTROL_NONE,
    GLX_SWAP_CONTROL_EXT,
    GLX_SWAP_CONTROL_SGI,
    GLX_SWAP_CONTROL_MESA
};

/* X context to associate a struct gl_drawable to an hwnd */
static XContext gl_hwnd_context;
/* X context to associate a struct gl_drawable to a pbuffer hdc */
static XContext gl_pbuffer_context;

static struct list context_list = LIST_INIT( context_list );
static struct list pbuffer_list = LIST_INIT( pbuffer_list );
static struct wgl_pixel_format *pixel_formats;
static int nb_pixel_formats, nb_onscreen_formats;
static BOOL use_render_texture_emulation = TRUE;

/* Selects the preferred GLX swap control method for use by wglSwapIntervalEXT */
static enum glx_swap_control_method swap_control_method = GLX_SWAP_CONTROL_NONE;
/* Set when GLX_EXT_swap_control_tear is supported, requires GLX_SWAP_CONTROL_EXT */
static BOOL has_swap_control_tear = FALSE;
static BOOL has_swap_method = FALSE;

static CRITICAL_SECTION context_section;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &context_section,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": context_section") }
};
static CRITICAL_SECTION context_section = { &critsect_debug, -1, 0, 0, 0, 0 };

static const BOOL is_win64 = sizeof(void *) > sizeof(int);

static struct opengl_funcs opengl_funcs;

#define USE_GL_FUNC(name) #name,
static const char *opengl_func_names[] = { ALL_WGL_FUNCS };
static const char *glu_func_names[] = { ALL_GLU_FUNCS }; /* CrossOver Hack 10798 */
#undef USE_GL_FUNC

static void X11DRV_WineGL_LoadExtensions(void);
static void init_pixel_formats( Display *display );
static BOOL glxRequireVersion(int requiredVersion);

static void dump_PIXELFORMATDESCRIPTOR(const PIXELFORMATDESCRIPTOR *ppfd) {
  TRACE("  - size / version : %d / %d\n", ppfd->nSize, ppfd->nVersion);
  TRACE("  - dwFlags : ");
#define TEST_AND_DUMP(t,tv) if ((t) & (tv)) TRACE(#tv " ")
  TEST_AND_DUMP(ppfd->dwFlags, PFD_DEPTH_DONTCARE);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_DOUBLEBUFFER);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_DOUBLEBUFFER_DONTCARE);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_DRAW_TO_WINDOW);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_DRAW_TO_BITMAP);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_GENERIC_ACCELERATED);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_GENERIC_FORMAT);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_NEED_PALETTE);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_NEED_SYSTEM_PALETTE);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_STEREO);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_STEREO_DONTCARE);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_SUPPORT_GDI);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_SUPPORT_OPENGL);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_SWAP_COPY);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_SWAP_EXCHANGE);
  TEST_AND_DUMP(ppfd->dwFlags, PFD_SWAP_LAYER_BUFFERS);
  /* PFD_SUPPORT_COMPOSITION is new in Vista, it is similar to composition
   * under X e.g. COMPOSITE + GLX_EXT_TEXTURE_FROM_PIXMAP. */
  TEST_AND_DUMP(ppfd->dwFlags, PFD_SUPPORT_COMPOSITION);
#undef TEST_AND_DUMP
  TRACE("\n");

  TRACE("  - iPixelType : ");
  switch (ppfd->iPixelType) {
  case PFD_TYPE_RGBA: TRACE("PFD_TYPE_RGBA"); break;
  case PFD_TYPE_COLORINDEX: TRACE("PFD_TYPE_COLORINDEX"); break;
  }
  TRACE("\n");

  TRACE("  - Color   : %d\n", ppfd->cColorBits);
  TRACE("  - Red     : %d\n", ppfd->cRedBits);
  TRACE("  - Green   : %d\n", ppfd->cGreenBits);
  TRACE("  - Blue    : %d\n", ppfd->cBlueBits);
  TRACE("  - Alpha   : %d\n", ppfd->cAlphaBits);
  TRACE("  - Accum   : %d\n", ppfd->cAccumBits);
  TRACE("  - Depth   : %d\n", ppfd->cDepthBits);
  TRACE("  - Stencil : %d\n", ppfd->cStencilBits);
  TRACE("  - Aux     : %d\n", ppfd->cAuxBuffers);

  TRACE("  - iLayerType : ");
  switch (ppfd->iLayerType) {
  case PFD_MAIN_PLANE: TRACE("PFD_MAIN_PLANE"); break;
  case PFD_OVERLAY_PLANE: TRACE("PFD_OVERLAY_PLANE"); break;
  case (BYTE)PFD_UNDERLAY_PLANE: TRACE("PFD_UNDERLAY_PLANE"); break;
  }
  TRACE("\n");
}

#define PUSH1(attribs,att)        do { attribs[nAttribs++] = (att); } while (0)
#define PUSH2(attribs,att,value)  do { attribs[nAttribs++] = (att); attribs[nAttribs++] = (value); } while(0)

/* GLX 1.0 */
static XVisualInfo* (*pglXChooseVisual)( Display *dpy, int screen, int *attribList );
static GLXContext (*pglXCreateContext)( Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct );
static void (*pglXDestroyContext)( Display *dpy, GLXContext ctx );
static Bool (*pglXMakeCurrent)( Display *dpy, GLXDrawable drawable, GLXContext ctx);
static void (*pglXCopyContext)( Display *dpy, GLXContext src, GLXContext dst, unsigned long mask );
static void (*pglXSwapBuffers)( Display *dpy, GLXDrawable drawable );
static Bool (*pglXQueryExtension)( Display *dpy, int *errorb, int *event );
static Bool (*pglXQueryVersion)( Display *dpy, int *maj, int *min );
static Bool (*pglXIsDirect)( Display *dpy, GLXContext ctx );
static GLXContext (*pglXGetCurrentContext)( void );
static GLXDrawable (*pglXGetCurrentDrawable)( void );
static void (*pglXWaitGL)( void );

/* GLX 1.1 */
static const char *(*pglXQueryExtensionsString)( Display *dpy, int screen );
static const char *(*pglXQueryServerString)( Display *dpy, int screen, int name );
static const char *(*pglXGetClientString)( Display *dpy, int name );

/* GLX 1.3 */
static GLXFBConfig *(*pglXChooseFBConfig)( Display *dpy, int screen, const int *attribList, int *nitems );
static int (*pglXGetFBConfigAttrib)( Display *dpy, GLXFBConfig config, int attribute, int *value );
static GLXFBConfig *(*pglXGetFBConfigs)( Display *dpy, int screen, int *nelements );
static XVisualInfo *(*pglXGetVisualFromFBConfig)( Display *dpy, GLXFBConfig config );
static GLXPbuffer (*pglXCreatePbuffer)( Display *dpy, GLXFBConfig config, const int *attribList );
static void (*pglXDestroyPbuffer)( Display *dpy, GLXPbuffer pbuf );
static void (*pglXQueryDrawable)( Display *dpy, GLXDrawable draw, int attribute, unsigned int *value );
static GLXContext (*pglXCreateNewContext)( Display *dpy, GLXFBConfig config, int renderType, GLXContext shareList, Bool direct );
static Bool (*pglXMakeContextCurrent)( Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx );
static GLXPixmap (*pglXCreatePixmap)( Display *dpy, GLXFBConfig config, Pixmap pixmap, const int *attrib_list );
static void (*pglXDestroyPixmap)( Display *dpy, GLXPixmap pixmap );
static GLXWindow (*pglXCreateWindow)( Display *dpy, GLXFBConfig config, Window win, const int *attrib_list );
static void (*pglXDestroyWindow)( Display *dpy, GLXWindow win );

/* GLX Extensions */
static GLXContext (*pglXCreateContextAttribsARB)(Display *dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list);
static void* (*pglXGetProcAddressARB)(const GLubyte *);
static void (*pglXSwapIntervalEXT)(Display *dpy, GLXDrawable drawable, int interval);
static int   (*pglXSwapIntervalSGI)(int);

/* NV GLX Extension */
static void* (*pglXAllocateMemoryNV)(GLsizei size, GLfloat readfreq, GLfloat writefreq, GLfloat priority);
static void  (*pglXFreeMemoryNV)(GLvoid *pointer);

/* MESA GLX Extensions */
static void (*pglXCopySubBufferMESA)(Display *dpy, GLXDrawable drawable, int x, int y, int width, int height);
static int (*pglXSwapIntervalMESA)(unsigned int interval);
static Bool (*pglXQueryCurrentRendererIntegerMESA)(int attribute, unsigned int *value);
static const char *(*pglXQueryCurrentRendererStringMESA)(int attribute);
static Bool (*pglXQueryRendererIntegerMESA)(Display *dpy, int screen, int renderer, int attribute, unsigned int *value);
static const char *(*pglXQueryRendererStringMESA)(Display *dpy, int screen, int renderer, int attribute);

/* OpenML GLX Extensions */
static Bool (*pglXWaitForSbcOML)( Display *dpy, GLXDrawable drawable,
        INT64 target_sbc, INT64 *ust, INT64 *msc, INT64 *sbc );
static INT64 (*pglXSwapBuffersMscOML)( Display *dpy, GLXDrawable drawable,
        INT64 target_msc, INT64 divisor, INT64 remainder );

#ifdef SONAME_LIBEGL

/* EGL */
static void *(*peglGetProcAddress)(const char *procname);

static EGLint (*peglGetError)(void);

static EGLDisplay (*peglGetDisplay)(EGLNativeDisplayType display_id);
static EGLBoolean (*peglInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
static EGLBoolean (*peglTerminate)(EGLDisplay dpy);

static const char *(*peglQueryString)(EGLDisplay dpy, EGLint name);

static EGLBoolean (*peglGetConfigs)(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
static EGLBoolean (*peglChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list,
        EGLConfig *configs, EGLint config_size, EGLint *num_config);
static EGLBoolean (*peglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);

static EGLSurface (*peglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config,
        EGLNativeWindowType win, const EGLint *attrib_list);
static EGLSurface (*peglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
static EGLSurface (*peglCreatePixmapSurface)(EGLDisplay dpy, EGLConfig config,
        EGLNativePixmapType pixmap, const EGLint *attrib_list);
static EGLBoolean (*peglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
static EGLBoolean (*peglQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);

static EGLBoolean (*peglBindAPI)(EGLenum api);
static EGLenum (*peglQueryAPI)(void);

static EGLBoolean (*peglWaitClient)(void);

static EGLBoolean (*peglReleaseThread)(void);

static EGLSurface (*peglCreatePbufferFromClientBuffer)(EGLDisplay dpy, EGLenum buftype,
        EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list);

static EGLBoolean (*peglSurfaceAttrib)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value);
static EGLBoolean (*peglBindTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
static EGLBoolean (*peglReleaseTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);

static EGLBoolean (*peglSwapInterval)(EGLDisplay dpy, EGLint interval);

static EGLContext (*peglCreateContext)(EGLDisplay dpy, EGLConfig config,
        EGLContext share_context, const EGLint *attrib_list);
static EGLBoolean (*peglDestroyContext)(EGLDisplay dpy, EGLContext ctx);
static EGLBoolean (*peglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);

static EGLContext (*peglGetCurrentContext)(void);
static EGLSurface (*peglGetCurrentSurface)(EGLint readdraw);
static EGLDisplay (*peglGetCurrentDisplay)(void);
static EGLBoolean (*peglQueryContext)(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value);

static EGLBoolean (*peglWaitGL)(void);
static EGLBoolean (*peglWaitNative)(EGLint engine);
static EGLBoolean (*peglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
static EGLBoolean (*peglCopyBuffers)(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target);

static struct opengl_funcs egl_funcs;
static EGLDisplay display;

static BOOL has_egl(void);

#endif /* defined(SONAME_LIBEGL) */

/* Standard OpenGL */
static void (*pglFinish)(void);
static void (*pglFlush)(void);
static const GLubyte *(*pglGetString)(GLenum name);

static void wglFinish(void);
static void wglFlush(void);
static const GLubyte *wglGetString(GLenum name);

/* check if the extension is present in the list */
static BOOL has_extension( const char *list, const char *ext )
{
    size_t len = strlen( ext );

    while (list)
    {
        while (*list == ' ') list++;
        if (!strncmp( list, ext, len ) && (!list[len] || list[len] == ' ')) return TRUE;
        list = strchr( list, ' ' );
    }
    return FALSE;
}

static int GLXErrorHandler(Display *dpy, XErrorEvent *event, void *arg)
{
    /* In the future we might want to find the exact X or GLX error to report back to the app */
    return 1;
}

static BOOL X11DRV_WineGL_InitOpenglInfo(void)
{
    static const char legacy_extensions[] = " WGL_EXT_extensions_string WGL_EXT_swap_control";

    int screen = DefaultScreen(gdi_display);
    Window win = 0, root = 0;
    const char *gl_version;
    const char *gl_renderer;
    const char* str;
    BOOL glx_direct;
    XVisualInfo *vis;
    GLXContext ctx = NULL;
    XSetWindowAttributes attr;
    BOOL ret = FALSE;
    int attribList[] = {GLX_RGBA, GLX_DOUBLEBUFFER, None};

    attr.override_redirect = True;
    attr.colormap = None;
    attr.border_pixel = 0;

    vis = pglXChooseVisual(gdi_display, screen, attribList);
    if (vis) {
#ifdef __i386__
        WORD old_fs = wine_get_fs();
        /* Create a GLX Context. Without one we can't query GL information */
        ctx = pglXCreateContext(gdi_display, vis, None, GL_TRUE);
        if (wine_get_fs() != old_fs)
        {
            wine_set_fs( old_fs );
            ERR( "%%fs register corrupted, probably broken ATI driver, disabling OpenGL.\n" );
            ERR( "You need to set the \"UseFastTls\" option to \"2\" in your X config file.\n" );
            goto done;
        }
#else
        ctx = pglXCreateContext(gdi_display, vis, None, GL_TRUE);
#endif
    }
    if (!ctx) goto done;

    root = RootWindow( gdi_display, vis->screen );
    if (vis->visual != DefaultVisual( gdi_display, vis->screen ))
        attr.colormap = XCreateColormap( gdi_display, root, vis->visual, AllocNone );
    if ((win = XCreateWindow( gdi_display, root, -1, -1, 1, 1, 0, vis->depth, InputOutput,
                              vis->visual, CWBorderPixel | CWOverrideRedirect | CWColormap, &attr )))
        XMapWindow( gdi_display, win );
    else
        win = root;

    if(pglXMakeCurrent(gdi_display, win, ctx) == 0)
    {
        ERR_(winediag)( "Unable to activate OpenGL context, most likely your %s OpenGL drivers haven't been "
                        "installed correctly\n", is_win64 ? "64-bit" : "32-bit" );
        goto done;
    }
    gl_renderer = (const char *)opengl_funcs.gl.p_glGetString(GL_RENDERER);
    gl_version  = (const char *)opengl_funcs.gl.p_glGetString(GL_VERSION);
    str = (const char *) opengl_funcs.gl.p_glGetString(GL_EXTENSIONS);
    glExtensions = HeapAlloc(GetProcessHeap(), 0, strlen(str)+sizeof(legacy_extensions));
    strcpy(glExtensions, str);
    strcat(glExtensions, legacy_extensions);

    /* Get the common GLX version supported by GLX client and server ( major/minor) */
    pglXQueryVersion(gdi_display, &glxVersion[0], &glxVersion[1]);

    glxExtensions = pglXQueryExtensionsString(gdi_display, screen);
    glx_direct = pglXIsDirect(gdi_display, ctx);

    TRACE("GL version             : %s.\n", gl_version);
    TRACE("GL renderer            : %s.\n", gl_renderer);
    TRACE("GLX version            : %d.%d.\n", glxVersion[0], glxVersion[1]);
    TRACE("Server GLX version     : %s.\n", pglXQueryServerString(gdi_display, screen, GLX_VERSION));
    TRACE("Server GLX vendor:     : %s.\n", pglXQueryServerString(gdi_display, screen, GLX_VENDOR));
    TRACE("Client GLX version     : %s.\n", pglXGetClientString(gdi_display, GLX_VERSION));
    TRACE("Client GLX vendor:     : %s.\n", pglXGetClientString(gdi_display, GLX_VENDOR));
    TRACE("Direct rendering enabled: %s\n", glx_direct ? "True" : "False");

    if(!glx_direct)
    {
        int fd = ConnectionNumber(gdi_display);
        struct sockaddr_un uaddr;
        unsigned int uaddrlen = sizeof(struct sockaddr_un);

        /* In general indirect rendering on a local X11 server indicates a driver problem.
         * Detect a local X11 server by checking whether the X11 socket is a Unix socket.
         */
        if(!getsockname(fd, (struct sockaddr *)&uaddr, &uaddrlen) && uaddr.sun_family == AF_UNIX)
            ERR_(winediag)("Direct rendering is disabled, most likely your %s OpenGL drivers "
                           "haven't been installed correctly (using GL renderer %s, version %s).\n",
                           is_win64 ? "64-bit" : "32-bit", debugstr_a(gl_renderer),
                           debugstr_a(gl_version));
    }
    else
    {
        /* In general you would expect that if direct rendering is returned, that you receive hardware
         * accelerated OpenGL rendering. The definition of direct rendering is that rendering is performed
         * client side without sending all GL commands to X using the GLX protocol. When Mesa falls back to
         * software rendering, it shows direct rendering.
         *
         * Depending on the cause of software rendering a different rendering string is shown. In case Mesa fails
         * to load a DRI module 'Software Rasterizer' is returned. When Mesa is compiled as a OpenGL reference driver
         * it shows 'Mesa X11'.
         */
        if(!strcmp(gl_renderer, "Software Rasterizer") || !strcmp(gl_renderer, "Mesa X11"))
            ERR_(winediag)("The Mesa OpenGL driver is using software rendering, most likely your %s OpenGL "
                           "drivers haven't been installed correctly (using GL renderer %s, version %s).\n",
                           is_win64 ? "64-bit" : "32-bit", debugstr_a(gl_renderer),
                           debugstr_a(gl_version));
    }
    ret = TRUE;

done:
    if(vis) XFree(vis);
    if(ctx) {
        pglXMakeCurrent(gdi_display, None, NULL);    
        pglXDestroyContext(gdi_display, ctx);
    }
    if (win != root) XDestroyWindow( gdi_display, win );
    if (attr.colormap) XFreeColormap( gdi_display, attr.colormap );
    if (!ret) ERR(" couldn't initialize OpenGL, expect problems\n");
    return ret;
}

static void *opengl_handle;

static BOOL WINAPI init_opengl( INIT_ONCE *once, void *param, void **context )
{
    void *glu_handle;
    char buffer[200];
    int error_base, event_base;
    unsigned int i;

    /* No need to load any other libraries as according to the ABI, libGL should be self-sufficient
       and include all dependencies */
    opengl_handle = wine_dlopen(SONAME_LIBGL, RTLD_NOW|RTLD_GLOBAL, buffer, sizeof(buffer));
    if (opengl_handle == NULL)
    {
        ERR( "Failed to load libGL: %s\n", buffer );
        ERR( "OpenGL support is disabled.\n");
        return TRUE;
    }

    for (i = 0; i < sizeof(opengl_func_names)/sizeof(opengl_func_names[0]); i++)
    {
        if (!(((void **)&opengl_funcs.gl)[i] = wine_dlsym( opengl_handle, opengl_func_names[i], NULL, 0 )))
        {
            ERR( "%s not found in libGL, disabling OpenGL.\n", opengl_func_names[i] );
            goto failed;
        }
    }

#ifdef SONAME_LIBGLU /* CrossOver Hack 10798 */
    glu_handle = wine_dlopen(SONAME_LIBGLU, RTLD_NOW|RTLD_GLOBAL, buffer, sizeof(buffer));
    if (glu_handle)
    {
        for (i = 0; i < sizeof(glu_func_names)/sizeof(glu_func_names[0]); i++)
        {
            if (!(((void **)&opengl_funcs.glu)[i] = wine_dlsym( glu_handle, glu_func_names[i], NULL, 0 )))
                WARN( "%s not found in libGLU\n", glu_func_names[i] );
        }
    }
    else
        WARN( "Failed to load libGLU: %s\n", buffer );
#endif

    /* redirect some standard OpenGL functions */
#define REDIRECT(func) \
    do { p##func = opengl_funcs.gl.p_##func; opengl_funcs.gl.p_##func = w##func; } while(0)
    REDIRECT( glFinish );
    REDIRECT( glFlush );
    REDIRECT( glGetString );
#undef REDIRECT

    pglXGetProcAddressARB = wine_dlsym(opengl_handle, "glXGetProcAddressARB", NULL, 0);
    if (pglXGetProcAddressARB == NULL) {
        ERR("Could not find glXGetProcAddressARB in libGL, disabling OpenGL.\n");
        goto failed;
    }

#define LOAD_FUNCPTR(f) do if((p##f = (void*)pglXGetProcAddressARB((const unsigned char*)#f)) == NULL) \
    { \
        ERR( "%s not found in libGL, disabling OpenGL.\n", #f ); \
        goto failed; \
    } while(0)

    /* GLX 1.0 */
    LOAD_FUNCPTR(glXChooseVisual);
    LOAD_FUNCPTR(glXCopyContext);
    LOAD_FUNCPTR(glXCreateContext);
    LOAD_FUNCPTR(glXGetCurrentContext);
    LOAD_FUNCPTR(glXGetCurrentDrawable);
    LOAD_FUNCPTR(glXDestroyContext);
    LOAD_FUNCPTR(glXIsDirect);
    LOAD_FUNCPTR(glXMakeCurrent);
    LOAD_FUNCPTR(glXSwapBuffers);
    LOAD_FUNCPTR(glXWaitGL);
    LOAD_FUNCPTR(glXQueryExtension);
    LOAD_FUNCPTR(glXQueryVersion);

    /* GLX 1.1 */
    LOAD_FUNCPTR(glXGetClientString);
    LOAD_FUNCPTR(glXQueryExtensionsString);
    LOAD_FUNCPTR(glXQueryServerString);

    /* GLX 1.3 */
    LOAD_FUNCPTR(glXCreatePbuffer);
    LOAD_FUNCPTR(glXCreateNewContext);
    LOAD_FUNCPTR(glXDestroyPbuffer);
    LOAD_FUNCPTR(glXMakeContextCurrent);
    LOAD_FUNCPTR(glXGetFBConfigs);
    LOAD_FUNCPTR(glXCreatePixmap);
    LOAD_FUNCPTR(glXDestroyPixmap);
    LOAD_FUNCPTR(glXCreateWindow);
    LOAD_FUNCPTR(glXDestroyWindow);
#undef LOAD_FUNCPTR

/* It doesn't matter if these fail. They'll only be used if the driver reports
   the associated extension is available (and if a driver reports the extension
   is available but fails to provide the functions, it's quite broken) */
#define LOAD_FUNCPTR(f) p##f = pglXGetProcAddressARB((const GLubyte *)#f)
    /* ARB GLX Extension */
    LOAD_FUNCPTR(glXCreateContextAttribsARB);
    /* EXT GLX Extension */
    LOAD_FUNCPTR(glXSwapIntervalEXT);
    /* MESA GLX Extension */
    LOAD_FUNCPTR(glXSwapIntervalMESA);
    /* SGI GLX Extension */
    LOAD_FUNCPTR(glXSwapIntervalSGI);
    /* NV GLX Extension */
    LOAD_FUNCPTR(glXAllocateMemoryNV);
    LOAD_FUNCPTR(glXFreeMemoryNV);
#undef LOAD_FUNCPTR

    if(!X11DRV_WineGL_InitOpenglInfo()) goto failed;

    if (pglXQueryExtension(gdi_display, &error_base, &event_base)) {
        TRACE("GLX is up and running error_base = %d\n", error_base);
    } else {
        ERR( "GLX extension is missing, disabling OpenGL.\n" );
        goto failed;
    }
    gl_hwnd_context = XUniqueContext();
    gl_pbuffer_context = XUniqueContext();

    /* In case of GLX you have direct and indirect rendering. Most of the time direct rendering is used
     * as in general only that is hardware accelerated. In some cases like in case of remote X indirect
     * rendering is used.
     *
     * The main problem for our OpenGL code is that we need certain GLX calls but their presence
     * depends on the reported GLX client / server version and on the client / server extension list.
     * Those don't have to be the same.
     *
     * In general the server GLX information lists the capabilities in case of indirect rendering.
     * When direct rendering is used, the OpenGL client library is responsible for which GLX calls are
     * available and in that case the client GLX informat can be used.
     * OpenGL programs should use the 'intersection' of both sets of information which is advertised
     * in the GLX version/extension list. When a program does this it works for certain for both
     * direct and indirect rendering.
     *
     * The problem we are having in this area is that ATI's Linux drivers are broken. For some reason
     * they haven't added some very important GLX extensions like GLX_SGIX_fbconfig to their client
     * extension list which causes this extension not to be listed. (Wine requires this extension).
     * ATI advertises a GLX client version of 1.3 which implies that this fbconfig extension among
     * pbuffers is around.
     *
     * In order to provide users of Ati's proprietary drivers with OpenGL support, we need to detect
     * the ATI drivers and from then on use GLX client information for them.
     */

    if(glxRequireVersion(3)) {
        pglXChooseFBConfig = pglXGetProcAddressARB((const GLubyte *) "glXChooseFBConfig");
        pglXGetFBConfigAttrib = pglXGetProcAddressARB((const GLubyte *) "glXGetFBConfigAttrib");
        pglXGetVisualFromFBConfig = pglXGetProcAddressARB((const GLubyte *) "glXGetVisualFromFBConfig");
        pglXQueryDrawable = pglXGetProcAddressARB((const GLubyte *) "glXQueryDrawable");
    } else if(
#ifdef __APPLE__
              TRUE ||
#endif
              has_extension( glxExtensions, "GLX_SGIX_fbconfig")) {
        pglXChooseFBConfig = pglXGetProcAddressARB((const GLubyte *) "glXChooseFBConfigSGIX");
        pglXGetFBConfigAttrib = pglXGetProcAddressARB((const GLubyte *) "glXGetFBConfigAttribSGIX");
        pglXGetVisualFromFBConfig = pglXGetProcAddressARB((const GLubyte *) "glXGetVisualFromFBConfigSGIX");

        /* The mesa libGL client library seems to forward glXQueryDrawable to the Xserver, so only
         * enable this function when the Xserver understand GLX 1.3 or newer
         */
        pglXQueryDrawable = NULL;
    } else if(strcmp("ATI", pglXGetClientString(gdi_display, GLX_VENDOR)) == 0) {
        TRACE("Overriding ATI GLX capabilities!\n");
        pglXChooseFBConfig = pglXGetProcAddressARB((const GLubyte *) "glXChooseFBConfig");
        pglXGetFBConfigAttrib = pglXGetProcAddressARB((const GLubyte *) "glXGetFBConfigAttrib");
        pglXGetVisualFromFBConfig = pglXGetProcAddressARB((const GLubyte *) "glXGetVisualFromFBConfig");
        pglXQueryDrawable = pglXGetProcAddressARB((const GLubyte *) "glXQueryDrawable");

        /* Use client GLX information in case of the ATI drivers. We override the
         * capabilities over here and not somewhere else as ATI might better their
         * life in the future. In case they release proper drivers this block of
         * code won't be called. */
        glxExtensions = pglXGetClientString(gdi_display, GLX_EXTENSIONS);
    } else {
        ERR(" glx_version is %s and GLX_SGIX_fbconfig extension is unsupported. Expect problems.\n",
            pglXQueryServerString(gdi_display, DefaultScreen(gdi_display), GLX_VERSION));
    }

    if (has_extension( glxExtensions, "GLX_MESA_copy_sub_buffer")) {
        pglXCopySubBufferMESA = pglXGetProcAddressARB((const GLubyte *) "glXCopySubBufferMESA");
    }

    if (has_extension( glxExtensions, "GLX_MESA_query_renderer" ))
    {
        pglXQueryCurrentRendererIntegerMESA = pglXGetProcAddressARB(
                (const GLubyte *)"glXQueryCurrentRendererIntegerMESA" );
        pglXQueryCurrentRendererStringMESA = pglXGetProcAddressARB(
                (const GLubyte *)"glXQueryCurrentRendererStringMESA" );
        pglXQueryRendererIntegerMESA = pglXGetProcAddressARB( (const GLubyte *)"glXQueryRendererIntegerMESA" );
        pglXQueryRendererStringMESA = pglXGetProcAddressARB( (const GLubyte *)"glXQueryRendererStringMESA" );
    }

    if (has_extension( glxExtensions, "GLX_OML_sync_control" ))
    {
        pglXWaitForSbcOML = pglXGetProcAddressARB( (const GLubyte *)"glXWaitForSbcOML" );
        pglXSwapBuffersMscOML = pglXGetProcAddressARB( (const GLubyte *)"glXSwapBuffersMscOML" );
    }

    X11DRV_WineGL_LoadExtensions();
    init_pixel_formats( gdi_display );
    return TRUE;

failed:
    wine_dlclose(opengl_handle, NULL, 0);
    opengl_handle = NULL;
    return TRUE;
}

static BOOL has_opengl(void)
{
    static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce(&init_once, init_opengl, NULL, NULL);
    return opengl_handle != NULL;
}

static const char *debugstr_fbconfig( GLXFBConfig fbconfig )
{
    int id, visual, drawable;

    if (pglXGetFBConfigAttrib( gdi_display, fbconfig, GLX_FBCONFIG_ID, &id ))
        return "*** invalid fbconfig";
    pglXGetFBConfigAttrib( gdi_display, fbconfig, GLX_VISUAL_ID, &visual );
    pglXGetFBConfigAttrib( gdi_display, fbconfig, GLX_DRAWABLE_TYPE, &drawable );
    return wine_dbg_sprintf( "fbconfig %#x visual id %#x drawable type %#x", id, visual, drawable );
}

static int ConvertAttribWGLtoGLX(const int* iWGLAttr, int* oGLXAttr, struct wgl_pbuffer* pbuf) {
  int nAttribs = 0;
  unsigned cur = 0; 
  int pop;
  int drawattrib = 0;
  int nvfloatattrib = GLX_DONT_CARE;
  int pixelattrib = GLX_DONT_CARE;

  /* The list of WGL attributes is allowed to be NULL. We don't return here for NULL
   * because we need to do fixups for GLX_DRAWABLE_TYPE/GLX_RENDER_TYPE/GLX_FLOAT_COMPONENTS_NV. */
  while (iWGLAttr && 0 != iWGLAttr[cur]) {
    TRACE("pAttr[%d] = %x\n", cur, iWGLAttr[cur]);

    switch (iWGLAttr[cur]) {
    case WGL_AUX_BUFFERS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_AUX_BUFFERS, pop);
      TRACE("pAttr[%d] = GLX_AUX_BUFFERS: %d\n", cur, pop);
      break;
    case WGL_COLOR_BITS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_BUFFER_SIZE, pop);
      TRACE("pAttr[%d] = GLX_BUFFER_SIZE: %d\n", cur, pop);
      break;
    case WGL_BLUE_BITS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_BLUE_SIZE, pop);
      TRACE("pAttr[%d] = GLX_BLUE_SIZE: %d\n", cur, pop);
      break;
    case WGL_RED_BITS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_RED_SIZE, pop);
      TRACE("pAttr[%d] = GLX_RED_SIZE: %d\n", cur, pop);
      break;
    case WGL_GREEN_BITS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_GREEN_SIZE, pop);
      TRACE("pAttr[%d] = GLX_GREEN_SIZE: %d\n", cur, pop);
      break;
    case WGL_ALPHA_BITS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_ALPHA_SIZE, pop);
      TRACE("pAttr[%d] = GLX_ALPHA_SIZE: %d\n", cur, pop);
      break;
    case WGL_DEPTH_BITS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_DEPTH_SIZE, pop);
      TRACE("pAttr[%d] = GLX_DEPTH_SIZE: %d\n", cur, pop);
      break;
    case WGL_STENCIL_BITS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_STENCIL_SIZE, pop);
      TRACE("pAttr[%d] = GLX_STENCIL_SIZE: %d\n", cur, pop);
      break;
    case WGL_DOUBLE_BUFFER_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_DOUBLEBUFFER, pop);
      TRACE("pAttr[%d] = GLX_DOUBLEBUFFER: %d\n", cur, pop);
      break;
    case WGL_STEREO_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_STEREO, pop);
      TRACE("pAttr[%d] = GLX_STEREO: %d\n", cur, pop);
      break;

    case WGL_PIXEL_TYPE_ARB:
      pop = iWGLAttr[++cur];
      TRACE("pAttr[%d] = WGL_PIXEL_TYPE_ARB: %d\n", cur, pop);
      switch (pop) {
      case WGL_TYPE_COLORINDEX_ARB: pixelattrib = GLX_COLOR_INDEX_BIT; break ;
      case WGL_TYPE_RGBA_ARB: pixelattrib = GLX_RGBA_BIT; break ;
      /* This is the same as WGL_TYPE_RGBA_FLOAT_ATI but the GLX constants differ, only the ARB GLX one is widely supported so use that */
      case WGL_TYPE_RGBA_FLOAT_ATI: pixelattrib = GLX_RGBA_FLOAT_BIT; break ;
      case WGL_TYPE_RGBA_UNSIGNED_FLOAT_EXT: pixelattrib = GLX_RGBA_UNSIGNED_FLOAT_BIT_EXT; break ;
      default:
        ERR("unexpected PixelType(%x)\n", pop);	
        pop = 0;
      }
      break;

    case WGL_SUPPORT_GDI_ARB:
      /* This flag is set in a pixel format */
      pop = iWGLAttr[++cur];
      TRACE("pAttr[%d] = WGL_SUPPORT_GDI_ARB: %d\n", cur, pop);
      break;

    case WGL_DRAW_TO_BITMAP_ARB:
      /* This flag is set in a pixel format */
      pop = iWGLAttr[++cur];
      TRACE("pAttr[%d] = WGL_DRAW_TO_BITMAP_ARB: %d\n", cur, pop);
      break;

    case WGL_DRAW_TO_WINDOW_ARB:
      pop = iWGLAttr[++cur];
      TRACE("pAttr[%d] = WGL_DRAW_TO_WINDOW_ARB: %d\n", cur, pop);
      /* GLX_DRAWABLE_TYPE flags need to be OR'd together. See below. */
      if (pop) {
        drawattrib |= GLX_WINDOW_BIT;
      }
      break;

    case WGL_DRAW_TO_PBUFFER_ARB:
      pop = iWGLAttr[++cur];
      TRACE("pAttr[%d] = WGL_DRAW_TO_PBUFFER_ARB: %d\n", cur, pop);
      /* GLX_DRAWABLE_TYPE flags need to be OR'd together. See below. */
      if (pop) {
        drawattrib |= GLX_PBUFFER_BIT;
      }
      break;

    case WGL_ACCELERATION_ARB:
      /* This flag is set in a pixel format */
      pop = iWGLAttr[++cur];
      TRACE("pAttr[%d] = WGL_ACCELERATION_ARB: %d\n", cur, pop);
      break;

    case WGL_SUPPORT_OPENGL_ARB:
      pop = iWGLAttr[++cur];
      /** nothing to do, if we are here, supposing support Accelerated OpenGL */
      TRACE("pAttr[%d] = WGL_SUPPORT_OPENGL_ARB: %d\n", cur, pop);
      break;

    case WGL_SWAP_METHOD_ARB:
      pop = iWGLAttr[++cur];
      TRACE("pAttr[%d] = WGL_SWAP_METHOD_ARB: %#x\n", cur, pop);
      if (has_swap_method)
      {
          switch (pop)
          {
          case WGL_SWAP_EXCHANGE_ARB:
              pop = GLX_SWAP_EXCHANGE_OML;
              break;
          case WGL_SWAP_COPY_ARB:
              pop = GLX_SWAP_COPY_OML;
              break;
          case WGL_SWAP_UNDEFINED_ARB:
              pop = GLX_SWAP_UNDEFINED_OML;
              break;
          default:
              ERR("Unexpected swap method %#x.\n", pop);
              pop = GLX_DONT_CARE;
          }
          PUSH2(oGLXAttr, GLX_SWAP_METHOD_OML, pop);
      }
      else
      {
          WARN("GLX_OML_swap_method not supported, ignoring attribute.\n");
      }
      break;

    case WGL_PBUFFER_LARGEST_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_LARGEST_PBUFFER, pop);
      TRACE("pAttr[%d] = GLX_LARGEST_PBUFFER: %x\n", cur, pop);
      break;

    case WGL_SAMPLE_BUFFERS_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_SAMPLE_BUFFERS_ARB, pop);
      TRACE("pAttr[%d] = GLX_SAMPLE_BUFFERS_ARB: %x\n", cur, pop);
      break;

    case WGL_SAMPLES_ARB:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_SAMPLES_ARB, pop);
      TRACE("pAttr[%d] = GLX_SAMPLES_ARB: %x\n", cur, pop);
      break;

    case WGL_TEXTURE_FORMAT_ARB:
    case WGL_TEXTURE_TARGET_ARB:
    case WGL_MIPMAP_TEXTURE_ARB:
      TRACE("WGL_render_texture Attributes: %x as %x\n", iWGLAttr[cur], iWGLAttr[cur + 1]);
      pop = iWGLAttr[++cur];
      if (NULL == pbuf) {
        ERR("trying to use GLX_Pbuffer Attributes without Pbuffer (was %x)\n", iWGLAttr[cur]);
      }
      if (!use_render_texture_emulation) {
        if (WGL_NO_TEXTURE_ARB != pop) {
          ERR("trying to use WGL_render_texture Attributes without support (was %x)\n", iWGLAttr[cur]);
          return -1; /** error: don't support it */
        } else {
          drawattrib |= GLX_PBUFFER_BIT;
        }
      }
      break ;
    case WGL_FLOAT_COMPONENTS_NV:
      nvfloatattrib = iWGLAttr[++cur];
      TRACE("pAttr[%d] = WGL_FLOAT_COMPONENTS_NV: %x\n", cur, nvfloatattrib);
      break ;
    case WGL_BIND_TO_TEXTURE_DEPTH_NV:
    case WGL_BIND_TO_TEXTURE_RGB_ARB:
    case WGL_BIND_TO_TEXTURE_RGBA_ARB:
    case WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_R_NV:
    case WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_RG_NV:
    case WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_RGB_NV:
    case WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_RGBA_NV:
      pop = iWGLAttr[++cur];
      /** cannot be converted, see direct handling on 
       *   - wglGetPixelFormatAttribivARB
       *  TODO: wglChoosePixelFormat
       */
      break ;
    case WGL_FRAMEBUFFER_SRGB_CAPABLE_EXT:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT, pop);
      TRACE("pAttr[%d] = GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT: %x\n", cur, pop);
      break ;

    case WGL_TYPE_RGBA_UNSIGNED_FLOAT_EXT:
      pop = iWGLAttr[++cur];
      PUSH2(oGLXAttr, GLX_RGBA_UNSIGNED_FLOAT_TYPE_EXT, pop);
      TRACE("pAttr[%d] = GLX_RGBA_UNSIGNED_FLOAT_TYPE_EXT: %x\n", cur, pop);
      break ;
    default:
      FIXME("unsupported %x WGL Attribute\n", iWGLAttr[cur]);
      cur++;
      break;
    }
    ++cur;
  }

  /* By default glXChooseFBConfig defaults to GLX_WINDOW_BIT. wglChoosePixelFormatARB searches through
   * all formats. Unless drawattrib is set to a non-zero value override it with GLX_DONT_CARE, so that
   * pixmap and pbuffer formats appear as well. */
  if (!drawattrib) drawattrib = GLX_DONT_CARE;
  PUSH2(oGLXAttr, GLX_DRAWABLE_TYPE, drawattrib);
  TRACE("pAttr[?] = GLX_DRAWABLE_TYPE: %#x\n", drawattrib);

  /* By default glXChooseFBConfig uses GLX_RGBA_BIT as the default value. Since wglChoosePixelFormatARB
   * searches in all formats we have to do the same. For this reason we set GLX_RENDER_TYPE to
   * GLX_DONT_CARE unless it is overridden. */
  PUSH2(oGLXAttr, GLX_RENDER_TYPE, pixelattrib);
  TRACE("pAttr[?] = GLX_RENDER_TYPE: %#x\n", pixelattrib);

  /* Set GLX_FLOAT_COMPONENTS_NV all the time */
  if (has_extension(glxExtensions, "GLX_NV_float_buffer")) {
    PUSH2(oGLXAttr, GLX_FLOAT_COMPONENTS_NV, nvfloatattrib);
    TRACE("pAttr[?] = GLX_FLOAT_COMPONENTS_NV: %#x\n", nvfloatattrib);
  }

  return nAttribs;
}

static int get_render_type_from_fbconfig(Display *display, GLXFBConfig fbconfig)
{
    int render_type, render_type_bit;
    pglXGetFBConfigAttrib(display, fbconfig, GLX_RENDER_TYPE, &render_type_bit);
    switch(render_type_bit)
    {
        case GLX_RGBA_BIT:
            render_type = GLX_RGBA_TYPE;
            break;
        case GLX_COLOR_INDEX_BIT:
            render_type = GLX_COLOR_INDEX_TYPE;
            break;
        case GLX_RGBA_FLOAT_BIT:
            render_type = GLX_RGBA_FLOAT_TYPE;
            break;
        case GLX_RGBA_UNSIGNED_FLOAT_BIT_EXT:
            render_type = GLX_RGBA_UNSIGNED_FLOAT_TYPE_EXT;
            break;
        default:
            ERR("Unknown render_type: %x\n", render_type_bit);
            render_type = 0;
    }
    return render_type;
}

/* Check whether a fbconfig is suitable for Windows-style bitmap rendering */
static BOOL check_fbconfig_bitmap_capability(Display *display, GLXFBConfig fbconfig)
{
    int dbuf, value;
    pglXGetFBConfigAttrib(display, fbconfig, GLX_DOUBLEBUFFER, &dbuf);
    pglXGetFBConfigAttrib(gdi_display, fbconfig, GLX_DRAWABLE_TYPE, &value);

    /* Windows only supports bitmap rendering on single buffered formats, further the fbconfig needs to have
     * the GLX_PIXMAP_BIT set. */
    return !dbuf && (value & GLX_PIXMAP_BIT);
}

static void init_pixel_formats( Display *display )
{
    struct wgl_pixel_format *list;
    int size = 0, onscreen_size = 0;
    int fmt_id, nCfgs, i, run, bmp_formats;
    GLXFBConfig* cfgs;
    XVisualInfo *visinfo;

    cfgs = pglXGetFBConfigs(display, DefaultScreen(display), &nCfgs);
    if (NULL == cfgs || 0 == nCfgs) {
        if(cfgs != NULL) XFree(cfgs);
        ERR("glXChooseFBConfig returns NULL\n");
        return;
    }

    /* Bitmap rendering on Windows implies the use of the Microsoft GDI software renderer.
     * Further most GLX drivers only offer pixmap rendering using indirect rendering (except for modern drivers which support 'AIGLX' / composite).
     * Indirect rendering can indicate software rendering (on Nvidia it is hw accelerated)
     * Since bitmap rendering implies the use of software rendering we can safely use indirect rendering for bitmaps.
     *
     * Below we count the number of formats which are suitable for bitmap rendering. Windows restricts bitmap rendering to single buffered formats.
     */
    for(i=0, bmp_formats=0; i<nCfgs; i++)
    {
        if(check_fbconfig_bitmap_capability(display, cfgs[i]))
            bmp_formats++;
    }
    TRACE("Found %d bitmap capable fbconfigs\n", bmp_formats);

    list = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (nCfgs + bmp_formats) * sizeof(*list));

    /* Fill the pixel format list. Put onscreen formats at the top and offscreen ones at the bottom.
     * Do this as GLX doesn't guarantee that the list is sorted */
    for(run=0; run < 2; run++)
    {
        for(i=0; i<nCfgs; i++) {
            pglXGetFBConfigAttrib(display, cfgs[i], GLX_FBCONFIG_ID, &fmt_id);
            visinfo = pglXGetVisualFromFBConfig(display, cfgs[i]);

            /* The first run we only add onscreen formats (ones which have an associated X Visual).
             * The second run we only set offscreen formats. */
            if(!run && visinfo)
            {
                /* We implement child window rendering using offscreen buffers (using composite or an XPixmap).
                 * The contents is copied to the destination using XCopyArea. For the copying to work
                 * the depth of the source and destination window should be the same. In general this should
                 * not be a problem for OpenGL as drivers only advertise formats with a similar depth (or no depth).
                 * As of the introduction of composition managers at least Nvidia now also offers ARGB visuals
                 * with a depth of 32 in addition to the default 24 bit. In order to prevent BadMatch errors we only
                 * list formats with the same depth. */
                if(visinfo->depth != default_visual.depth)
                {
                    XFree(visinfo);
                    continue;
                }

                TRACE("Found onscreen format FBCONFIG_ID 0x%x corresponding to iPixelFormat %d at GLX index %d\n", fmt_id, size+1, i);
                list[size].config.fbc = cfgs[i];
                list[size].visual = visinfo;
                list[size].fmt_id = fmt_id;
                list[size].render_type = get_render_type_from_fbconfig(display, cfgs[i]);
                list[size].dwFlags = 0;
                size++;
                onscreen_size++;

                /* Clone a format if it is bitmap capable for indirect rendering to bitmaps */
                if(check_fbconfig_bitmap_capability(display, cfgs[i]))
                {
                    TRACE("Found bitmap capable format FBCONFIG_ID 0x%x corresponding to iPixelFormat %d at GLX index %d\n", fmt_id, size+1, i);
                    list[size].config.fbc = cfgs[i];
                    list[size].visual = visinfo;
                    list[size].fmt_id = fmt_id;
                    list[size].render_type = get_render_type_from_fbconfig(display, cfgs[i]);
                    list[size].dwFlags = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_GDI | PFD_GENERIC_FORMAT;
                    size++;
                    onscreen_size++;
                }
            } else if(run && !visinfo) {
                int window_drawable=0;
                pglXGetFBConfigAttrib(gdi_display, cfgs[i], GLX_DRAWABLE_TYPE, &window_drawable);

                /* Recent Nvidia drivers and DRI drivers offer window drawable formats without a visual.
                 * This are formats like 16-bit rgb on a 24-bit desktop. In order to support these formats
                 * onscreen we would have to use glXCreateWindow instead of XCreateWindow. Further it will
                 * likely make our child window opengl rendering more complicated since likely you can't use
                 * XCopyArea on a GLX Window.
                 * For now ignore fbconfigs which are window drawable but lack a visual. */
                if(window_drawable & GLX_WINDOW_BIT)
                {
                    TRACE("Skipping FBCONFIG_ID 0x%x as an offscreen format because it is window_drawable\n", fmt_id);
                    continue;
                }

                TRACE("Found offscreen format FBCONFIG_ID 0x%x corresponding to iPixelFormat %d at GLX index %d\n", fmt_id, size+1, i);
                list[size].config.fbc = cfgs[i];
                list[size].fmt_id = fmt_id;
                list[size].render_type = get_render_type_from_fbconfig(display, cfgs[i]);
                list[size].dwFlags = 0;
                size++;
            }
            else if (visinfo) XFree(visinfo);
        }
    }

    XFree(cfgs);

    pixel_formats = list;
    nb_pixel_formats = size;
    nb_onscreen_formats = onscreen_size;
}

static inline BOOL is_valid_pixel_format( int format )
{
    return format > 0 && format <= nb_pixel_formats;
}

static inline BOOL is_onscreen_pixel_format( int format )
{
    return format > 0 && format <= nb_onscreen_formats;
}

static inline int pixel_format_index( const struct wgl_pixel_format *format )
{
    return format - pixel_formats + 1;
}

/* GLX can advertise dozens of different pixelformats including offscreen and onscreen ones.
 * In our WGL implementation we only support a subset of these formats namely the format of
 * Wine's main visual and offscreen formats (if they are available).
 * This function converts a WGL format to its corresponding GLX one.
 */
static const struct wgl_pixel_format *get_pixel_format(Display *display, int iPixelFormat, BOOL AllowOffscreen)
{
    /* Check if the pixelformat is valid. Note that it is legal to pass an invalid
     * iPixelFormat in case of probing the number of pixelformats.
     */
    if (is_valid_pixel_format( iPixelFormat ) &&
        (is_onscreen_pixel_format( iPixelFormat ) || AllowOffscreen)) {
        TRACE("Returning fmt_id=%#x for iPixelFormat=%d\n",
              pixel_formats[iPixelFormat-1].fmt_id, iPixelFormat);
        return &pixel_formats[iPixelFormat-1];
    }
    return NULL;
}

static struct gl_drawable *grab_gl_drawable( struct gl_drawable *gl )
{
    InterlockedIncrement( &gl->ref );
    return gl;
}

static void release_gl_drawable( struct gl_drawable *gl )
{
#ifdef SONAME_LIBEGL
    BOOL egl = has_egl();
#endif

    if (!gl) return;
    if (InterlockedDecrement( &gl->ref )) return;
    switch (gl->type)
    {
    case DC_GL_WINDOW:
    case DC_GL_CHILD_WIN:
#ifdef SONAME_LIBEGL
        if (egl)
        {
            TRACE( "destroying %lx drawable %p\n", gl->window, gl->drawable.s );
            peglDestroySurface( display, gl->drawable.s );
        }
        else
        {
#endif
            TRACE( "destroying %lx drawable %lx\n", gl->window, gl->drawable.d );
            pglXDestroyWindow( gdi_display, gl->drawable.d );
#ifdef SONAME_LIBEGL
        }
#endif
        XDestroyWindow( gdi_display, gl->window );
        break;
    case DC_GL_PIXMAP_WIN:
#ifdef SONAME_LIBEGL
        if (egl)
        {
            TRACE( "destroying pixmap %lx drawable %p\n", gl->pixmap, gl->drawable.s );
            peglDestroySurface( display, gl->drawable.s );
        }
        else
        {
#endif
            TRACE( "destroying pixmap %lx drawable %lx\n", gl->pixmap, gl->drawable.d );
            pglXDestroyPixmap( gdi_display, gl->drawable.d );
#ifdef SONAME_LIBEGL
        }
#endif
        XFreePixmap( gdi_display, gl->pixmap );
        break;
    default:
        break;
    }
    HeapFree( GetProcessHeap(), 0, gl );
}

/* Mark any allocated context using the drawable 'old' to use 'new' */
static void mark_drawable_dirty( struct gl_drawable *old, struct gl_drawable *new )
{
    struct wgl_context *ctx;

    EnterCriticalSection( &context_section );
    LIST_FOR_EACH_ENTRY( ctx, &context_list, struct wgl_context, entry )
    {
        if (old == ctx->drawables[0] || old == ctx->new_drawables[0])
        {
            release_gl_drawable( ctx->new_drawables[0] );
            ctx->new_drawables[0] = grab_gl_drawable( new );
        }
        if (old == ctx->drawables[1] || old == ctx->new_drawables[1])
        {
            release_gl_drawable( ctx->new_drawables[1] );
            ctx->new_drawables[1] = grab_gl_drawable( new );
        }
    }
    LeaveCriticalSection( &context_section );
}

/* Given the current context, make sure its drawable is sync'd */
static inline void sync_context(struct wgl_context *context)
{
    BOOL refresh = FALSE;

    EnterCriticalSection( &context_section );
    if (context->new_drawables[0])
    {
        release_gl_drawable( context->drawables[0] );
        context->drawables[0] = context->new_drawables[0];
        context->new_drawables[0] = NULL;
        refresh = TRUE;
    }
    if (context->new_drawables[1])
    {
        release_gl_drawable( context->drawables[1] );
        context->drawables[1] = context->new_drawables[1];
        context->new_drawables[1] = NULL;
        refresh = TRUE;
    }
    if (refresh)
    {
        if (glxRequireVersion(3))
            pglXMakeContextCurrent(gdi_display, context->drawables[0]->drawable.d,
                                   context->drawables[1]->drawable.d, context->ctx.glx);
        else
            pglXMakeCurrent(gdi_display, context->drawables[0]->drawable.d, context->ctx.glx);
    }
    LeaveCriticalSection( &context_section );
}

static BOOL set_swap_interval(GLXDrawable drawable, int interval)
{
    BOOL ret = TRUE;

    switch (swap_control_method)
    {
    case GLX_SWAP_CONTROL_EXT:
        X11DRV_expect_error(gdi_display, GLXErrorHandler, NULL);
        pglXSwapIntervalEXT(gdi_display, drawable, interval);
        XSync(gdi_display, False);
        ret = !X11DRV_check_error();
        break;

    case GLX_SWAP_CONTROL_MESA:
        ret = !pglXSwapIntervalMESA(interval);
        break;

    case GLX_SWAP_CONTROL_SGI:
        /* wglSwapIntervalEXT considers an interval value of zero to mean that
         * vsync should be disabled, but glXSwapIntervalSGI considers such a
         * value to be an error. Just silently ignore the request for now.
         */
        if (!interval)
            WARN("Request to disable vertical sync is not handled\n");
        else
            ret = !pglXSwapIntervalSGI(interval);
        break;

    case GLX_SWAP_CONTROL_NONE:
        /* Unlikely to happen on modern GLX implementations */
        WARN("Request to adjust swap interval is not handled\n");
        break;
    }

    return ret;
}

static struct gl_drawable *get_gl_drawable( HWND hwnd, HDC hdc )
{
    struct gl_drawable *gl;

    EnterCriticalSection( &context_section );
    if (hwnd && !XFindContext( gdi_display, (XID)hwnd, gl_hwnd_context, (char **)&gl ))
        gl = grab_gl_drawable( gl );
    else if (hdc && !XFindContext( gdi_display, (XID)hdc, gl_pbuffer_context, (char **)&gl ))
        gl = grab_gl_drawable( gl );
    else
        gl = NULL;
    LeaveCriticalSection( &context_section );
    return gl;
}

static GLXContext create_glxcontext(Display *display, struct wgl_context *context, GLXContext shareList)
{
    GLXContext ctx;

    if(context->gl3_context)
    {
        if(context->numAttribs)
            ctx = pglXCreateContextAttribsARB(gdi_display, context->fmt->config.fbc,
                    shareList, GL_TRUE, context->attribList);
        else
            ctx = pglXCreateContextAttribsARB(gdi_display, context->fmt->config.fbc, shareList, GL_TRUE, NULL);
    }
    else if(context->fmt->visual)
        ctx = pglXCreateContext(gdi_display, context->fmt->visual, shareList, GL_TRUE);
    else /* Create a GLX Context for a pbuffer */
        ctx = pglXCreateNewContext(gdi_display, context->fmt->config.fbc, context->fmt->render_type, shareList, TRUE);

    return ctx;
}


/***********************************************************************
 *              create_gl_drawable
 */
static struct gl_drawable *create_gl_drawable( HWND hwnd, const struct wgl_pixel_format *format )
{
    struct gl_drawable *gl, *prev;
    XVisualInfo *visual = format->visual;
    RECT rect;
    int width, height;

    GetClientRect( hwnd, &rect );
    width  = min( max( 1, rect.right ), 65535 );
    height = min( max( 1, rect.bottom ), 65535 );

    if (!(gl = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*gl) ))) return NULL;

    /* Default GLX and WGL swap interval is 1, but in case of glXSwapIntervalSGI
     * there is no way to query it, so we have to store it here.
     */
    gl->swap_interval = 1;
    gl->refresh_swap_interval = TRUE;
    gl->format = format;
    gl->ref = 1;

    if (GetAncestor( hwnd, GA_PARENT ) == GetDesktopWindow())  /* top-level window */
    {
        gl->type = DC_GL_WINDOW;
        gl->window = create_client_window( hwnd, visual );
        if (gl->window)
            gl->drawable.d = pglXCreateWindow( gdi_display, gl->format->config.fbc, gl->window, NULL );
        TRACE( "%p created client %lx drawable %lx\n", hwnd, gl->window, gl->drawable.d );
    }
#ifdef SONAME_LIBXCOMPOSITE
    else if(usexcomposite)
    {
        gl->type = DC_GL_CHILD_WIN;
        gl->window = create_client_window( hwnd, visual );
        if (gl->window)
        {
            gl->drawable.d = pglXCreateWindow( gdi_display, gl->format->config.fbc, gl->window, NULL );
            pXCompositeRedirectWindow( gdi_display, gl->window, CompositeRedirectManual );
        }
        TRACE( "%p created child %lx drawable %lx\n", hwnd, gl->window, gl->drawable.d );
    }
#endif
    else
    {
        WARN("XComposite is not available, using GLXPixmap hack\n");

        gl->type = DC_GL_PIXMAP_WIN;
        gl->pixmap = XCreatePixmap( gdi_display, root_window, width, height, visual->depth );
        if (gl->pixmap)
        {
            gl->drawable.d = pglXCreatePixmap( gdi_display, gl->format->config.fbc, gl->pixmap, NULL );
            if (!gl->drawable.d) XFreePixmap( gdi_display, gl->pixmap );
            gl->pixmap_size.cx = width;
            gl->pixmap_size.cy = height;
        }
    }

    if (!gl->drawable.d)
    {
        HeapFree( GetProcessHeap(), 0, gl );
        return NULL;
    }

    EnterCriticalSection( &context_section );
    if (!XFindContext( gdi_display, (XID)hwnd, gl_hwnd_context, (char **)&prev ))
    {
        gl->swap_interval = prev->swap_interval;
        release_gl_drawable( prev );
    }
    XSaveContext( gdi_display, (XID)hwnd, gl_hwnd_context, (char *)grab_gl_drawable(gl) );
    LeaveCriticalSection( &context_section );
    return gl;
}


/***********************************************************************
 *              set_win_format
 */
static BOOL set_win_format( HWND hwnd, const struct wgl_pixel_format *format )
{
    struct gl_drawable *gl;

    if (!format->visual) return FALSE;

    if (!(gl = create_gl_drawable( hwnd, format ))) return FALSE;

    TRACE( "created GL drawable %lx for win %p %s\n",
           gl->drawable.d, hwnd, debugstr_fbconfig( format->config.fbc ));

    XFlush( gdi_display );
    release_gl_drawable( gl );

    __wine_set_pixel_format( hwnd, pixel_format_index( format ));
    return TRUE;
}


static BOOL set_pixel_format(HDC hdc, int format, BOOL allow_change)
{
    const struct wgl_pixel_format *fmt;
    int value;
    HWND hwnd = WindowFromDC( hdc );

    TRACE("(%p,%d)\n", hdc, format);

    if (!hwnd || hwnd == GetDesktopWindow())
    {
        WARN( "not a valid window DC %p/%p\n", hdc, hwnd );
        return FALSE;
    }

    fmt = get_pixel_format(gdi_display, format, FALSE /* Offscreen */);
    if (!fmt)
    {
        ERR( "Invalid format %d\n", format );
        return FALSE;
    }

    pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_DRAWABLE_TYPE, &value);
    if (!(value & GLX_WINDOW_BIT))
    {
        WARN( "Pixel format %d is not compatible for window rendering\n", format );
        return FALSE;
    }

    if (!allow_change)
    {
        struct gl_drawable *gl;
        if ((gl = get_gl_drawable( hwnd, hdc )))
        {
            int prev = pixel_format_index( gl->format );
            release_gl_drawable( gl );
            return prev == format;  /* cannot change it if already set */
        }
    }

    return set_win_format( hwnd, fmt );
}


/***********************************************************************
 *              sync_gl_drawable
 */
void sync_gl_drawable( HWND hwnd )
{
    struct gl_drawable *old, *new;

    if (!(old = get_gl_drawable( hwnd, 0 ))) return;

    switch (old->type)
    {
    case DC_GL_PIXMAP_WIN:
        if (!(new = create_gl_drawable( hwnd, old->format ))) break;
        mark_drawable_dirty( old, new );
        XFlush( gdi_display );
        TRACE( "Recreated GL drawable %lx to replace %lx\n", new->drawable.d, old->drawable.d );
        release_gl_drawable( new );
        break;
    default:
        break;
    }
    release_gl_drawable( old );
}

/**
 * glxdrv_DescribePixelFormat
 *
 * Get the pixel-format descriptor associated to the given id
 */
static int glxdrv_wglDescribePixelFormat( HDC hdc, int iPixelFormat,
                                          UINT nBytes, PIXELFORMATDESCRIPTOR *ppfd)
{
  /*XVisualInfo *vis;*/
  int value;
  int rb,gb,bb,ab;
  const struct wgl_pixel_format *fmt;

  if (!has_opengl()) return 0;

  TRACE("(%p,%d,%d,%p)\n", hdc, iPixelFormat, nBytes, ppfd);

  if (!ppfd) return nb_onscreen_formats;

  /* Look for the iPixelFormat in our list of supported formats. If it is supported we get the index in the FBConfig table and the number of supported formats back */
  fmt = get_pixel_format(gdi_display, iPixelFormat, FALSE /* Offscreen */);
  if (!fmt) {
      WARN("unexpected format %d\n", iPixelFormat);
      return 0;
  }

  if (nBytes < sizeof(PIXELFORMATDESCRIPTOR)) {
    ERR("Wrong structure size !\n");
    /* Should set error */
    return 0;
  }

  memset(ppfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
  ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
  ppfd->nVersion = 1;

  /* These flags are always the same... */
  ppfd->dwFlags = PFD_SUPPORT_OPENGL;
  /* Now the flags extracted from the Visual */

  pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_DRAWABLE_TYPE, &value);
  if(value & GLX_WINDOW_BIT)
      ppfd->dwFlags |= PFD_DRAW_TO_WINDOW;

  /* On Windows bitmap rendering is only offered using the GDI Software renderer. We reserve some formats (see get_formats for more info)
   * for bitmap rendering since we require indirect rendering for this. Further pixel format logs of a GeforceFX, Geforce8800GT, Radeon HD3400 and a
   * Radeon 9000 indicated that all bitmap formats have PFD_SUPPORT_GDI. Except for 2 formats on the Radeon 9000 none of the hw accelerated formats
   * offered the GDI bit either. */
  ppfd->dwFlags |= fmt->dwFlags & (PFD_DRAW_TO_BITMAP | PFD_SUPPORT_GDI);

  /* PFD_GENERIC_FORMAT - gdi software rendering
   * PFD_GENERIC_ACCELERATED - some parts are accelerated by a display driver (MCD e.g. 3dfx minigl)
   * none set - full hardware accelerated by a ICD
   *
   * We only set PFD_GENERIC_FORMAT on bitmap formats (see get_formats) as that's what ATI and Nvidia Windows drivers do  */
  ppfd->dwFlags |= fmt->dwFlags & (PFD_GENERIC_FORMAT | PFD_GENERIC_ACCELERATED);

  pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_DOUBLEBUFFER, &value);
  if (value) {
      ppfd->dwFlags |= PFD_DOUBLEBUFFER;
      ppfd->dwFlags &= ~PFD_SUPPORT_GDI;
  }
  pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_STEREO, &value); if (value) ppfd->dwFlags |= PFD_STEREO;

  /* Pixel type */
  pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_RENDER_TYPE, &value);
  if (value & GLX_RGBA_BIT)
    ppfd->iPixelType = PFD_TYPE_RGBA;
  else
    ppfd->iPixelType = PFD_TYPE_COLORINDEX;

  /* Color bits */
  pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_BUFFER_SIZE, &value);
  ppfd->cColorBits = value;

  /* Red, green, blue and alpha bits / shifts */
  if (ppfd->iPixelType == PFD_TYPE_RGBA) {
    pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_RED_SIZE, &rb);
    pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_GREEN_SIZE, &gb);
    pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_BLUE_SIZE, &bb);
    pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_ALPHA_SIZE, &ab);

    ppfd->cRedBits = rb;
    ppfd->cRedShift = gb + bb + ab;
    ppfd->cBlueBits = bb;
    ppfd->cBlueShift = ab;
    ppfd->cGreenBits = gb;
    ppfd->cGreenShift = bb + ab;
    ppfd->cAlphaBits = ab;
    ppfd->cAlphaShift = 0;
  } else {
    ppfd->cRedBits = 0;
    ppfd->cRedShift = 0;
    ppfd->cBlueBits = 0;
    ppfd->cBlueShift = 0;
    ppfd->cGreenBits = 0;
    ppfd->cGreenShift = 0;
    ppfd->cAlphaBits = 0;
    ppfd->cAlphaShift = 0;
  }

  /* Accum RGBA bits */
  pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_ACCUM_RED_SIZE, &rb);
  pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_ACCUM_GREEN_SIZE, &gb);
  pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_ACCUM_BLUE_SIZE, &bb);
  pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_ACCUM_ALPHA_SIZE, &ab);

  ppfd->cAccumBits = rb+gb+bb+ab;
  ppfd->cAccumRedBits = rb;
  ppfd->cAccumGreenBits = gb;
  ppfd->cAccumBlueBits = bb;
  ppfd->cAccumAlphaBits = ab;

  /* Aux bits */
  pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_AUX_BUFFERS, &value);
  ppfd->cAuxBuffers = value;

  /* Depth bits */
  pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_DEPTH_SIZE, &value);
  ppfd->cDepthBits = value;

  /* stencil bits */
  pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_STENCIL_SIZE, &value);
  ppfd->cStencilBits = value;

  ppfd->iLayerType = PFD_MAIN_PLANE;

  if (TRACE_ON(wgl)) {
    dump_PIXELFORMATDESCRIPTOR(ppfd);
  }

  return nb_onscreen_formats;
}

/***********************************************************************
 *		glxdrv_wglGetPixelFormat
 */
static int glxdrv_wglGetPixelFormat( HDC hdc )
{
    struct gl_drawable *gl;
    int ret = 0;

    if ((gl = get_gl_drawable( WindowFromDC( hdc ), hdc )))
    {
        ret = pixel_format_index( gl->format );
        /* Offscreen formats can't be used with traditional WGL calls.
         * As has been verified on Windows GetPixelFormat doesn't fail but returns iPixelFormat=1. */
        if (!is_onscreen_pixel_format( ret )) ret = 1;
        release_gl_drawable( gl );
    }
    TRACE( "%p -> %d\n", hdc, ret );
    return ret;
}

/***********************************************************************
 *		glxdrv_wglSetPixelFormat
 */
static BOOL glxdrv_wglSetPixelFormat( HDC hdc, int iPixelFormat, const PIXELFORMATDESCRIPTOR *ppfd )
{
    return set_pixel_format(hdc, iPixelFormat, FALSE);
}

/***********************************************************************
 *		glxdrv_wglCopyContext
 */
static BOOL glxdrv_wglCopyContext(struct wgl_context *src, struct wgl_context *dst, UINT mask)
{
    TRACE("%p -> %p mask %#x\n", src, dst, mask);

    pglXCopyContext(gdi_display, src->ctx.glx, dst->ctx.glx, mask);

    /* As opposed to wglCopyContext, glXCopyContext doesn't return anything, so hopefully we passed */
    return TRUE;
}

/***********************************************************************
 *		glxdrv_wglCreateContext
 */
static struct wgl_context *glxdrv_wglCreateContext( HDC hdc )
{
    struct wgl_context *ret;
    struct gl_drawable *gl;

    if (!(gl = get_gl_drawable( WindowFromDC( hdc ), hdc )))
    {
        SetLastError( ERROR_INVALID_PIXEL_FORMAT );
        return NULL;
    }

    if ((ret = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*ret))))
    {
        ret->hdc = hdc;
        ret->fmt = gl->format;
        ret->ctx.glx = create_glxcontext(gdi_display, ret, NULL);
        EnterCriticalSection( &context_section );
        list_add_head( &context_list, &ret->entry );
        LeaveCriticalSection( &context_section );
    }
    release_gl_drawable( gl );
    TRACE( "%p -> %p\n", hdc, ret );
    return ret;
}

/***********************************************************************
 *		glxdrv_wglDeleteContext
 */
static BOOL glxdrv_wglDeleteContext(struct wgl_context *ctx)
{
    struct wgl_pbuffer *pb;

    TRACE("(%p)\n", ctx);

    EnterCriticalSection( &context_section );
    list_remove( &ctx->entry );
    LIST_FOR_EACH_ENTRY( pb, &pbuffer_list, struct wgl_pbuffer, entry )
    {
        if (pb->prev_context == ctx->ctx.glx)
        {
            pglXDestroyContext(gdi_display, pb->tmp_context);
            pb->prev_context = pb->tmp_context = NULL;
        }
    }
    LeaveCriticalSection( &context_section );

    if (ctx->ctx.glx) pglXDestroyContext( gdi_display, ctx->ctx.glx );
    release_gl_drawable( ctx->drawables[0] );
    release_gl_drawable( ctx->drawables[1] );
    release_gl_drawable( ctx->new_drawables[0] );
    release_gl_drawable( ctx->new_drawables[1] );
    return HeapFree( GetProcessHeap(), 0, ctx );
}

/***********************************************************************
 *		glxdrv_wglGetProcAddress
 */
static PROC glxdrv_wglGetProcAddress(LPCSTR lpszProc)
{
    if (!strncmp(lpszProc, "wgl", 3)) return NULL;
    return pglXGetProcAddressARB((const GLubyte*)lpszProc);
}

static void set_context_drawables( struct wgl_context *ctx, struct gl_drawable *draw,
                                   struct gl_drawable *read )
{
    struct gl_drawable *prev[4];
    int i;

    prev[0] = ctx->drawables[0];
    prev[1] = ctx->drawables[1];
    prev[2] = ctx->new_drawables[0];
    prev[3] = ctx->new_drawables[1];
    ctx->drawables[0] = grab_gl_drawable( draw );
    ctx->drawables[1] = read ? grab_gl_drawable( read ) : NULL;
    ctx->new_drawables[0] = ctx->new_drawables[1] = NULL;
    for (i = 0; i < 4; i++) release_gl_drawable( prev[i] );
}

/***********************************************************************
 *		glxdrv_wglMakeCurrent
 */
static BOOL glxdrv_wglMakeCurrent(HDC hdc, struct wgl_context *ctx)
{
    BOOL ret = FALSE;
    struct gl_drawable *gl;

    TRACE("(%p,%p)\n", hdc, ctx);

    if (!ctx)
    {
        pglXMakeCurrent(gdi_display, None, NULL);
        NtCurrentTeb()->glContext = NULL;
        return TRUE;
    }

    if ((gl = get_gl_drawable( WindowFromDC( hdc ), hdc )))
    {
        if (ctx->fmt != gl->format)
        {
            WARN( "mismatched pixel format hdc %p %p ctx %p %p\n", hdc, gl->format, ctx, ctx->fmt );
            SetLastError( ERROR_INVALID_PIXEL_FORMAT );
            goto done;
        }

        TRACE("hdc %p drawable %lx fmt %p ctx %p %s\n", hdc, gl->drawable.d, gl->format, ctx->ctx.glx,
              debugstr_fbconfig( gl->format->config.fbc ));

        EnterCriticalSection( &context_section );
        ret = pglXMakeCurrent(gdi_display, gl->drawable.d, ctx->ctx.glx);
        if (ret)
        {
            NtCurrentTeb()->glContext = ctx;
            ctx->has_been_current = TRUE;
            ctx->hdc = hdc;
            set_context_drawables( ctx, gl, gl );
            ctx->refresh_drawables = FALSE;
            LeaveCriticalSection( &context_section );
            goto done;
        }
        LeaveCriticalSection( &context_section );
    }
    SetLastError( ERROR_INVALID_HANDLE );

done:
    release_gl_drawable( gl );
    TRACE( "%p,%p returning %d\n", hdc, ctx, ret );
    return ret;
}

/***********************************************************************
 *		X11DRV_wglMakeContextCurrentARB
 */
static BOOL X11DRV_wglMakeContextCurrentARB( HDC draw_hdc, HDC read_hdc, struct wgl_context *ctx )
{
    BOOL ret = FALSE;
    struct gl_drawable *draw_gl, *read_gl = NULL;

    TRACE("(%p,%p,%p)\n", draw_hdc, read_hdc, ctx);

    if (!ctx)
    {
        pglXMakeCurrent(gdi_display, None, NULL);
        NtCurrentTeb()->glContext = NULL;
        return TRUE;
    }

    if (!pglXMakeContextCurrent) return FALSE;

    if ((draw_gl = get_gl_drawable( WindowFromDC( draw_hdc ), draw_hdc )))
    {
        read_gl = get_gl_drawable( WindowFromDC( read_hdc ), read_hdc );

        EnterCriticalSection( &context_section );
        ret = pglXMakeContextCurrent(gdi_display, draw_gl->drawable.d,
                                     read_gl ? read_gl->drawable.d : 0, ctx->ctx.glx);
        if (ret)
        {
            ctx->has_been_current = TRUE;
            ctx->hdc = draw_hdc;
            set_context_drawables( ctx, draw_gl, read_gl );
            ctx->refresh_drawables = FALSE;
            NtCurrentTeb()->glContext = ctx;
            LeaveCriticalSection( &context_section );
            goto done;
        }
        LeaveCriticalSection( &context_section );
    }
    SetLastError( ERROR_INVALID_HANDLE );
done:
    release_gl_drawable( read_gl );
    release_gl_drawable( draw_gl );
    TRACE( "%p,%p,%p returning %d\n", draw_hdc, read_hdc, ctx, ret );
    return ret;
}

/***********************************************************************
 *		glxdrv_wglShareLists
 */
static BOOL glxdrv_wglShareLists(struct wgl_context *org, struct wgl_context *dest)
{
    TRACE("(%p, %p)\n", org, dest);

    /* Sharing of display lists works differently in GLX and WGL. In case of GLX it is done
     * at context creation time but in case of WGL it is done using wglShareLists.
     * In the past we tried to emulate wglShareLists by delaying GLX context creation until
     * either a wglMakeCurrent or wglShareLists. This worked fine for most apps but it causes
     * issues for OpenGL 3 because there wglCreateContextAttribsARB can fail in a lot of cases,
     * so there delaying context creation doesn't work.
     *
     * The new approach is to create a GLX context in wglCreateContext / wglCreateContextAttribsARB
     * and when a program requests sharing we recreate the destination context if it hasn't been made
     * current or when it hasn't shared display lists before.
     */

    if((org->has_been_current && dest->has_been_current) || dest->has_been_current)
    {
        ERR("Could not share display lists, one of the contexts has been current already !\n");
        return FALSE;
    }
    else if(dest->sharing)
    {
        ERR("Could not share display lists because hglrc2 has already shared lists before\n");
        return FALSE;
    }
    else
    {
        /* Re-create the GLX context and share display lists */
        pglXDestroyContext(gdi_display, dest->ctx.glx);
        dest->ctx.glx = create_glxcontext(gdi_display, dest, org->ctx.glx);
        TRACE(" re-created context (%p) for Wine context %p (%s) sharing lists with ctx %p (%s)\n",
              dest->ctx.glx, dest, debugstr_fbconfig(dest->fmt->config.fbc),
              org->ctx.glx, debugstr_fbconfig( org->fmt->config.fbc));

        org->sharing = TRUE;
        dest->sharing = TRUE;
        return TRUE;
    }
    return FALSE;
}

static void wglFinish(void)
{
    struct x11drv_escape_flush_gl_drawable escape;
    struct gl_drawable *gl;
    struct wgl_context *ctx = NtCurrentTeb()->glContext;

    escape.code = X11DRV_FLUSH_GL_DRAWABLE;
    escape.gl_drawable = 0;
    escape.flush = FALSE;

    if ((gl = get_gl_drawable( WindowFromDC( ctx->hdc ), 0 )))
    {
        switch (gl->type)
        {
        case DC_GL_PIXMAP_WIN: escape.gl_drawable = gl->pixmap; break;
        case DC_GL_CHILD_WIN:  escape.gl_drawable = gl->window; break;
        default: break;
        }
        sync_context(ctx);
        release_gl_drawable( gl );
    }

    pglFinish();
    if (escape.gl_drawable) ExtEscape( ctx->hdc, X11DRV_ESCAPE, sizeof(escape), (LPSTR)&escape, 0, NULL );
}

static void wglFlush(void)
{
    struct x11drv_escape_flush_gl_drawable escape;
    struct gl_drawable *gl;
    struct wgl_context *ctx = NtCurrentTeb()->glContext;

    escape.code = X11DRV_FLUSH_GL_DRAWABLE;
    escape.gl_drawable = 0;
    escape.flush = FALSE;

    if ((gl = get_gl_drawable( WindowFromDC( ctx->hdc ), 0 )))
    {
        switch (gl->type)
        {
        case DC_GL_PIXMAP_WIN: escape.gl_drawable = gl->pixmap; break;
        case DC_GL_CHILD_WIN:  escape.gl_drawable = gl->window; break;
        default: break;
        }
        sync_context(ctx);
        release_gl_drawable( gl );
    }

    pglFlush();
    if (escape.gl_drawable) ExtEscape( ctx->hdc, X11DRV_ESCAPE, sizeof(escape), (LPSTR)&escape, 0, NULL );
}

static const GLubyte *wglGetString(GLenum name)
{
    if (name == GL_EXTENSIONS && glExtensions) return (const GLubyte *)glExtensions;
    return pglGetString(name);
}

/***********************************************************************
 *		X11DRV_wglCreateContextAttribsARB
 */
static struct wgl_context *X11DRV_wglCreateContextAttribsARB( HDC hdc, struct wgl_context *hShareContext,
                                                              const int* attribList )
{
    struct wgl_context *ret;
    struct gl_drawable *gl;
    int err = 0;

    TRACE("(%p %p %p)\n", hdc, hShareContext, attribList);

    if (!(gl = get_gl_drawable( WindowFromDC( hdc ), hdc )))
    {
        SetLastError( ERROR_INVALID_PIXEL_FORMAT );
        return NULL;
    }

    if ((ret = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*ret))))
    {
        ret->hdc = hdc;
        ret->fmt = gl->format;
        ret->gl3_context = TRUE;
        if (attribList)
        {
            int *pContextAttribList = &ret->attribList[0];
            /* attribList consists of pairs {token, value] terminated with 0 */
            while(attribList[0] != 0)
            {
                TRACE("%#x %#x\n", attribList[0], attribList[1]);
                switch(attribList[0])
                {
                case WGL_CONTEXT_MAJOR_VERSION_ARB:
                    pContextAttribList[0] = GLX_CONTEXT_MAJOR_VERSION_ARB;
                    pContextAttribList[1] = attribList[1];
                    pContextAttribList += 2;
                    ret->numAttribs++;
                    break;
                case WGL_CONTEXT_MINOR_VERSION_ARB:
                    pContextAttribList[0] = GLX_CONTEXT_MINOR_VERSION_ARB;
                    pContextAttribList[1] = attribList[1];
                    pContextAttribList += 2;
                    ret->numAttribs++;
                    break;
                case WGL_CONTEXT_LAYER_PLANE_ARB:
                    break;
                case WGL_CONTEXT_FLAGS_ARB:
                    pContextAttribList[0] = GLX_CONTEXT_FLAGS_ARB;
                    pContextAttribList[1] = attribList[1];
                    pContextAttribList += 2;
                    ret->numAttribs++;
                    break;
                case WGL_CONTEXT_OPENGL_NO_ERROR_ARB:
                    pContextAttribList[0] = GLX_CONTEXT_OPENGL_NO_ERROR_ARB;
                    pContextAttribList[1] = attribList[1];
                    pContextAttribList += 2;
                    ret->numAttribs++;
                    break;
                case WGL_CONTEXT_PROFILE_MASK_ARB:
                    pContextAttribList[0] = GLX_CONTEXT_PROFILE_MASK_ARB;
                    pContextAttribList[1] = attribList[1];
                    pContextAttribList += 2;
                    ret->numAttribs++;
                    break;
                case WGL_RENDERER_ID_WINE:
                    pContextAttribList[0] = GLX_RENDERER_ID_MESA;
                    pContextAttribList[1] = attribList[1];
                    pContextAttribList += 2;
                    ret->numAttribs++;
                    break;
                default:
                    ERR("Unhandled attribList pair: %#x %#x\n", attribList[0], attribList[1]);
                }
                attribList += 2;
            }
        }

        X11DRV_expect_error(gdi_display, GLXErrorHandler, NULL);
        ret->ctx.glx = create_glxcontext(gdi_display, ret, hShareContext ? hShareContext->ctx.glx : NULL);
        XSync(gdi_display, False);
        if ((err = X11DRV_check_error()) || !ret->ctx.glx)
        {
            /* In the future we should convert the GLX error to a win32 one here if needed */
            WARN("Context creation failed (error %#x).\n", err);
            HeapFree( GetProcessHeap(), 0, ret );
            ret = NULL;
        }
        else
        {
            EnterCriticalSection( &context_section );
            list_add_head( &context_list, &ret->entry );
            LeaveCriticalSection( &context_section );
        }
    }

    release_gl_drawable( gl );
    TRACE( "%p -> %p\n", hdc, ret );
    return ret;
}

/**
 * X11DRV_wglGetExtensionsStringARB
 *
 * WGL_ARB_extensions_string: wglGetExtensionsStringARB
 */
static const char *X11DRV_wglGetExtensionsStringARB(HDC hdc)
{
    TRACE("() returning \"%s\"\n", wglExtensions);
    return wglExtensions;
}

/**
 * X11DRV_wglCreatePbufferARB
 *
 * WGL_ARB_pbuffer: wglCreatePbufferARB
 */
static struct wgl_pbuffer *X11DRV_wglCreatePbufferARB( HDC hdc, int iPixelFormat, int iWidth, int iHeight,
                                                       const int *piAttribList )
{
    struct wgl_pbuffer* object;
    const struct wgl_pixel_format *fmt;
    int attribs[256];
    int nAttribs = 0;

    TRACE("(%p, %d, %d, %d, %p)\n", hdc, iPixelFormat, iWidth, iHeight, piAttribList);

    /* Convert the WGL pixelformat to a GLX format, if it fails then the format is invalid */
    fmt = get_pixel_format(gdi_display, iPixelFormat, TRUE /* Offscreen */);
    if(!fmt) {
        ERR("(%p): invalid pixel format %d\n", hdc, iPixelFormat);
        SetLastError(ERROR_INVALID_PIXEL_FORMAT);
        return NULL;
    }

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (NULL == object) {
        SetLastError(ERROR_NO_SYSTEM_RESOURCES);
        return NULL;
    }
    object->width = iWidth;
    object->height = iHeight;
    object->fmt = fmt;

    PUSH2(attribs, GLX_PBUFFER_WIDTH,  iWidth);
    PUSH2(attribs, GLX_PBUFFER_HEIGHT, iHeight); 
    while (piAttribList && 0 != *piAttribList) {
        int attr_v;
        switch (*piAttribList) {
            case WGL_PBUFFER_LARGEST_ARB: {
                ++piAttribList;
                attr_v = *piAttribList;
                TRACE("WGL_LARGEST_PBUFFER_ARB = %d\n", attr_v);
                PUSH2(attribs, GLX_LARGEST_PBUFFER, attr_v);
                break;
            }

            case WGL_TEXTURE_FORMAT_ARB: {
                ++piAttribList;
                attr_v = *piAttribList;
                TRACE("WGL_render_texture Attribute: WGL_TEXTURE_FORMAT_ARB as %x\n", attr_v);
                if (WGL_NO_TEXTURE_ARB == attr_v) {
                    object->use_render_texture = 0;
                } else {
                    if (!use_render_texture_emulation) {
                        SetLastError(ERROR_INVALID_DATA);
                        goto create_failed;
                    }
                    switch (attr_v) {
                        case WGL_TEXTURE_RGB_ARB:
                            object->use_render_texture = GL_RGB;
                            object->texture_bpp = 3;
                            object->texture_format = GL_RGB;
                            object->texture_type = GL_UNSIGNED_BYTE;
                            break;
                        case WGL_TEXTURE_RGBA_ARB:
                            object->use_render_texture = GL_RGBA;
                            object->texture_bpp = 4;
                            object->texture_format = GL_RGBA;
                            object->texture_type = GL_UNSIGNED_BYTE;
                            break;

                        /* WGL_FLOAT_COMPONENTS_NV */
                        case WGL_TEXTURE_FLOAT_R_NV:
                            object->use_render_texture = GL_FLOAT_R_NV;
                            object->texture_bpp = 4;
                            object->texture_format = GL_RED;
                            object->texture_type = GL_FLOAT;
                            break;
                        case WGL_TEXTURE_FLOAT_RG_NV:
                            object->use_render_texture = GL_FLOAT_RG_NV;
                            object->texture_bpp = 8;
                            object->texture_format = GL_LUMINANCE_ALPHA;
                            object->texture_type = GL_FLOAT;
                            break;
                        case WGL_TEXTURE_FLOAT_RGB_NV:
                            object->use_render_texture = GL_FLOAT_RGB_NV;
                            object->texture_bpp = 12;
                            object->texture_format = GL_RGB;
                            object->texture_type = GL_FLOAT;
                            break;
                        case WGL_TEXTURE_FLOAT_RGBA_NV:
                            object->use_render_texture = GL_FLOAT_RGBA_NV;
                            object->texture_bpp = 16;
                            object->texture_format = GL_RGBA;
                            object->texture_type = GL_FLOAT;
                            break;
                        default:
                            ERR("Unknown texture format: %x\n", attr_v);
                            SetLastError(ERROR_INVALID_DATA);
                            goto create_failed;
                    }
                }
                break;
            }

            case WGL_TEXTURE_TARGET_ARB: {
                ++piAttribList;
                attr_v = *piAttribList;
                TRACE("WGL_render_texture Attribute: WGL_TEXTURE_TARGET_ARB as %x\n", attr_v);
                if (WGL_NO_TEXTURE_ARB == attr_v) {
                    object->texture_target = 0;
                } else {
                    if (!use_render_texture_emulation) {
                        SetLastError(ERROR_INVALID_DATA);
                        goto create_failed;
                    }
                    switch (attr_v) {
                        case WGL_TEXTURE_CUBE_MAP_ARB: {
                            if (iWidth != iHeight) {
                                SetLastError(ERROR_INVALID_DATA);
                                goto create_failed;
                            }
                            object->texture_target = GL_TEXTURE_CUBE_MAP;
                            object->texture_bind_target = GL_TEXTURE_BINDING_CUBE_MAP;
                           break;
                        }
                        case WGL_TEXTURE_1D_ARB: {
                            if (1 != iHeight) {
                                SetLastError(ERROR_INVALID_DATA);
                                goto create_failed;
                            }
                            object->texture_target = GL_TEXTURE_1D;
                            object->texture_bind_target = GL_TEXTURE_BINDING_1D;
                            break;
                        }
                        case WGL_TEXTURE_2D_ARB: {
                            object->texture_target = GL_TEXTURE_2D;
                            object->texture_bind_target = GL_TEXTURE_BINDING_2D;
                            break;
                        }
                        case WGL_TEXTURE_RECTANGLE_NV: {
                            object->texture_target = GL_TEXTURE_RECTANGLE_NV;
                            object->texture_bind_target = GL_TEXTURE_BINDING_RECTANGLE_NV;
                            break;
                        }
                        default:
                            ERR("Unknown texture target: %x\n", attr_v);
                            SetLastError(ERROR_INVALID_DATA);
                            goto create_failed;
                    }
                }
                break;
            }

            case WGL_MIPMAP_TEXTURE_ARB: {
                ++piAttribList;
                attr_v = *piAttribList;
                TRACE("WGL_render_texture Attribute: WGL_MIPMAP_TEXTURE_ARB as %x\n", attr_v);
                if (!use_render_texture_emulation) {
                    SetLastError(ERROR_INVALID_DATA);
                    goto create_failed;
                }
                break;
            }
        }
        ++piAttribList;
    }

    PUSH1(attribs, None);
    object->pbuffer.drawable = pglXCreatePbuffer(gdi_display, fmt->config.fbc, attribs);
    TRACE("new Pbuffer drawable as %lx\n", object->pbuffer.drawable);
    if (!object->pbuffer.drawable) {
        SetLastError(ERROR_NO_SYSTEM_RESOURCES);
        goto create_failed; /* unexpected error */
    }
    EnterCriticalSection( &context_section );
    list_add_head( &pbuffer_list, &object->entry );
    LeaveCriticalSection( &context_section );
    TRACE("->(%p)\n", object);
    return object;

create_failed:
    HeapFree(GetProcessHeap(), 0, object);
    TRACE("->(FAILED)\n");
    return NULL;
}

/**
 * X11DRV_wglDestroyPbufferARB
 *
 * WGL_ARB_pbuffer: wglDestroyPbufferARB
 */
static BOOL X11DRV_wglDestroyPbufferARB( struct wgl_pbuffer *object )
{
    TRACE("(%p)\n", object);

    EnterCriticalSection( &context_section );
    list_remove( &object->entry );
    LeaveCriticalSection( &context_section );
    pglXDestroyPbuffer(gdi_display, object->pbuffer.drawable);
    if (object->tmp_context)
        pglXDestroyContext(gdi_display, object->tmp_context);
    HeapFree(GetProcessHeap(), 0, object);
    return GL_TRUE;
}

/**
 * X11DRV_wglGetPbufferDCARB
 *
 * WGL_ARB_pbuffer: wglGetPbufferDCARB
 */
static HDC X11DRV_wglGetPbufferDCARB( struct wgl_pbuffer *object )
{
    struct x11drv_escape_set_drawable escape;
    struct gl_drawable *gl, *prev;
    HDC hdc;

    hdc = CreateDCA( "DISPLAY", NULL, NULL, NULL );
    if (!hdc) return 0;

    if (!(gl = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*gl) )))
    {
        DeleteDC( hdc );
        return 0;
    }
    gl->type = DC_GL_PBUFFER;
    gl->drawable.d = object->pbuffer.drawable;
    gl->format = object->fmt;
    gl->ref = 1;

    EnterCriticalSection( &context_section );
    if (!XFindContext( gdi_display, (XID)hdc, gl_pbuffer_context, (char **)&prev ))
        release_gl_drawable( prev );
    XSaveContext( gdi_display, (XID)hdc, gl_pbuffer_context, (char *)gl );
    LeaveCriticalSection( &context_section );

    escape.code = X11DRV_SET_DRAWABLE;
    escape.drawable = object->pbuffer.drawable;
    escape.mode = IncludeInferiors;
    SetRect( &escape.dc_rect, 0, 0, object->width, object->height );
    ExtEscape( hdc, X11DRV_ESCAPE, sizeof(escape), (LPSTR)&escape, 0, NULL );

    TRACE( "(%p)->(%p)\n", object, hdc );
    return hdc;
}

/**
 * X11DRV_wglQueryPbufferARB
 *
 * WGL_ARB_pbuffer: wglQueryPbufferARB
 */
static BOOL X11DRV_wglQueryPbufferARB( struct wgl_pbuffer *object, int iAttribute, int *piValue )
{
    TRACE("(%p, 0x%x, %p)\n", object, iAttribute, piValue);

    switch (iAttribute) {
        case WGL_PBUFFER_WIDTH_ARB:
            pglXQueryDrawable(gdi_display, object->pbuffer.drawable, GLX_WIDTH, (unsigned int*) piValue);
            break;
        case WGL_PBUFFER_HEIGHT_ARB:
            pglXQueryDrawable(gdi_display, object->pbuffer.drawable, GLX_HEIGHT, (unsigned int*) piValue);
            break;

        case WGL_PBUFFER_LOST_ARB:
            /* GLX Pbuffers cannot be lost by default. We can support this by
             * setting GLX_PRESERVED_CONTENTS to False and using glXSelectEvent
             * to receive pixel buffer clobber events, however that may or may
             * not give any benefit */
            *piValue = GL_FALSE;
            break;

        case WGL_TEXTURE_FORMAT_ARB:
            if (!object->use_render_texture) {
                *piValue = WGL_NO_TEXTURE_ARB;
            } else {
                if (!use_render_texture_emulation) {
                    SetLastError(ERROR_INVALID_HANDLE);
                    return GL_FALSE;
                }
                switch(object->use_render_texture) {
                    case GL_RGB:
                        *piValue = WGL_TEXTURE_RGB_ARB;
                        break;
                    case GL_RGBA:
                        *piValue = WGL_TEXTURE_RGBA_ARB;
                        break;
                    /* WGL_FLOAT_COMPONENTS_NV */
                    case GL_FLOAT_R_NV:
                        *piValue = WGL_TEXTURE_FLOAT_R_NV;
                        break;
                    case GL_FLOAT_RG_NV:
                        *piValue = WGL_TEXTURE_FLOAT_RG_NV;
                        break;
                    case GL_FLOAT_RGB_NV:
                        *piValue = WGL_TEXTURE_FLOAT_RGB_NV;
                        break;
                    case GL_FLOAT_RGBA_NV:
                        *piValue = WGL_TEXTURE_FLOAT_RGBA_NV;
                        break;
                    default:
                        ERR("Unknown texture format: %x\n", object->use_render_texture);
                }
            }
            break;

        case WGL_TEXTURE_TARGET_ARB:
            if (!object->texture_target){
                *piValue = WGL_NO_TEXTURE_ARB;
            } else {
                if (!use_render_texture_emulation) {
                    SetLastError(ERROR_INVALID_DATA);
                    return GL_FALSE;
                }
                switch (object->texture_target) {
                    case GL_TEXTURE_1D:       *piValue = WGL_TEXTURE_1D_ARB; break;
                    case GL_TEXTURE_2D:       *piValue = WGL_TEXTURE_2D_ARB; break;
                    case GL_TEXTURE_CUBE_MAP: *piValue = WGL_TEXTURE_CUBE_MAP_ARB; break;
                    case GL_TEXTURE_RECTANGLE_NV: *piValue = WGL_TEXTURE_RECTANGLE_NV; break;
                }
            }
            break;

        case WGL_MIPMAP_TEXTURE_ARB:
            *piValue = GL_FALSE; /** don't support that */
            FIXME("unsupported WGL_ARB_render_texture attribute query for 0x%x\n", iAttribute);
            break;

        default:
            FIXME("unexpected attribute %x\n", iAttribute);
            break;
    }

    return GL_TRUE;
}

/**
 * X11DRV_wglReleasePbufferDCARB
 *
 * WGL_ARB_pbuffer: wglReleasePbufferDCARB
 */
static int X11DRV_wglReleasePbufferDCARB( struct wgl_pbuffer *object, HDC hdc )
{
    struct gl_drawable *gl;

    TRACE("(%p, %p)\n", object, hdc);

    EnterCriticalSection( &context_section );

    if (!XFindContext( gdi_display, (XID)hdc, gl_pbuffer_context, (char **)&gl ))
    {
        XDeleteContext( gdi_display, (XID)hdc, gl_pbuffer_context );
        release_gl_drawable( gl );
    }
    else hdc = 0;

    LeaveCriticalSection( &context_section );

    return hdc && DeleteDC(hdc);
}

/**
 * X11DRV_wglSetPbufferAttribARB
 *
 * WGL_ARB_pbuffer: wglSetPbufferAttribARB
 */
static BOOL X11DRV_wglSetPbufferAttribARB( struct wgl_pbuffer *object, const int *piAttribList )
{
    GLboolean ret = GL_FALSE;

    WARN("(%p, %p): alpha-testing, report any problem\n", object, piAttribList);

    if (!object->use_render_texture) {
        SetLastError(ERROR_INVALID_HANDLE);
        return GL_FALSE;
    }
    if (use_render_texture_emulation) {
        return GL_TRUE;
    }
    return ret;
}

/**
 * X11DRV_wglChoosePixelFormatARB
 *
 * WGL_ARB_pixel_format: wglChoosePixelFormatARB
 */
static BOOL X11DRV_wglChoosePixelFormatARB( HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList,
                                            UINT nMaxFormats, int *piFormats, UINT *nNumFormats )
{
    int attribs[256];
    int nAttribs = 0;
    GLXFBConfig* cfgs;
    int nCfgs = 0;
    int it;
    int fmt_id;
    int start, end;
    UINT pfmt_it = 0;
    int run;
    int i;
    DWORD dwFlags = 0;

    TRACE("(%p, %p, %p, %d, %p, %p): hackish\n", hdc, piAttribIList, pfAttribFList, nMaxFormats, piFormats, nNumFormats);
    if (NULL != pfAttribFList) {
        FIXME("unused pfAttribFList\n");
    }

    nAttribs = ConvertAttribWGLtoGLX(piAttribIList, attribs, NULL);
    if (-1 == nAttribs) {
        WARN("Cannot convert WGL to GLX attributes\n");
        return GL_FALSE;
    }
    PUSH1(attribs, None);

    /* There is no 1:1 mapping between GLX and WGL formats because we duplicate some GLX formats for bitmap rendering (see get_formats).
     * Flags like PFD_SUPPORT_GDI, PFD_DRAW_TO_BITMAP and others are a property of the pixel format. We don't query these attributes
     * using glXChooseFBConfig but we filter the result of glXChooseFBConfig later on.
     */
    for(i=0; piAttribIList[i] != 0; i+=2)
    {
        switch(piAttribIList[i])
        {
            case WGL_DRAW_TO_BITMAP_ARB:
                if(piAttribIList[i+1])
                    dwFlags |= PFD_DRAW_TO_BITMAP;
                break;
            case WGL_ACCELERATION_ARB:
                switch(piAttribIList[i+1])
                {
                    case WGL_NO_ACCELERATION_ARB:
                        dwFlags |= PFD_GENERIC_FORMAT;
                        break;
                    case WGL_GENERIC_ACCELERATION_ARB:
                        dwFlags |= PFD_GENERIC_ACCELERATED;
                        break;
                    case WGL_FULL_ACCELERATION_ARB:
                        /* Nothing to do */
                        break;
                }
                break;
            case WGL_SUPPORT_GDI_ARB:
                if(piAttribIList[i+1])
                    dwFlags |= PFD_SUPPORT_GDI;
                break;
        }
    }

    /* Search for FB configurations matching the requirements in attribs */
    cfgs = pglXChooseFBConfig(gdi_display, DefaultScreen(gdi_display), attribs, &nCfgs);
    if (NULL == cfgs) {
        WARN("Compatible Pixel Format not found\n");
        return GL_FALSE;
    }

    /* Loop through all matching formats and check if they are suitable.
     * Note that this function should at max return nMaxFormats different formats */
    for(run=0; run < 2; run++)
    {
        for (it = 0; it < nCfgs && pfmt_it < nMaxFormats; ++it)
        {
            if (pglXGetFBConfigAttrib(gdi_display, cfgs[it], GLX_FBCONFIG_ID, &fmt_id))
            {
                ERR("Failed to retrieve FBCONFIG_ID from GLXFBConfig, expect problems.\n");
                continue;
            }

            /* During the first run we only want onscreen formats and during the second only offscreen */
            start = run == 1 ? nb_onscreen_formats : 0;
            end = run == 1 ? nb_pixel_formats : nb_onscreen_formats;

            for (i = start; i < end; i++)
            {
                if (pixel_formats[i].fmt_id == fmt_id && (pixel_formats[i].dwFlags & dwFlags) == dwFlags)
                {
                    piFormats[pfmt_it++] = i + 1;
                    TRACE("at %d/%d found FBCONFIG_ID 0x%x (%d)\n",
                          it + 1, nCfgs, fmt_id, i + 1);
                    break;
                }
            }
        }
    }

    *nNumFormats = pfmt_it;
    /** free list */
    XFree(cfgs);
    return GL_TRUE;
}

/**
 * X11DRV_wglGetPixelFormatAttribivARB
 *
 * WGL_ARB_pixel_format: wglGetPixelFormatAttribivARB
 */
static BOOL X11DRV_wglGetPixelFormatAttribivARB( HDC hdc, int iPixelFormat, int iLayerPlane,
                                                 UINT nAttributes, const int *piAttributes, int *piValues )
{
    UINT i;
    const struct wgl_pixel_format *fmt;
    int hTest;
    int tmp;
    int curGLXAttr = 0;

    TRACE("(%p, %d, %d, %d, %p, %p)\n", hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, piValues);

    if (0 < iLayerPlane) {
        FIXME("unsupported iLayerPlane(%d) > 0, returns FALSE\n", iLayerPlane);
        return GL_FALSE;
    }

    /* Convert the WGL pixelformat to a GLX one, if this fails then most likely the iPixelFormat isn't supported.
    * We don't have to fail yet as a program can specify an invalid iPixelFormat (lets say 0) if it wants to query
    * the number of supported WGL formats. Whether the iPixelFormat is valid is handled in the for-loop below. */
    fmt = get_pixel_format(gdi_display, iPixelFormat, TRUE /* Offscreen */);
    if(!fmt) {
        WARN("Unable to convert iPixelFormat %d to a GLX one!\n", iPixelFormat);
    }

    for (i = 0; i < nAttributes; ++i) {
        const int curWGLAttr = piAttributes[i];
        TRACE("pAttr[%d] = %x\n", i, curWGLAttr);

        switch (curWGLAttr) {
            case WGL_NUMBER_PIXEL_FORMATS_ARB:
                piValues[i] = nb_pixel_formats;
                continue;

            case WGL_SUPPORT_OPENGL_ARB:
                piValues[i] = GL_TRUE; 
                continue;

            case WGL_ACCELERATION_ARB:
                curGLXAttr = GLX_CONFIG_CAVEAT;
                if (!fmt) goto pix_error;
                if(fmt->dwFlags & PFD_GENERIC_FORMAT)
                    piValues[i] = WGL_NO_ACCELERATION_ARB;
                else if(fmt->dwFlags & PFD_GENERIC_ACCELERATED)
                    piValues[i] = WGL_GENERIC_ACCELERATION_ARB;
                else
                    piValues[i] = WGL_FULL_ACCELERATION_ARB;
                continue;

            case WGL_TRANSPARENT_ARB:
                curGLXAttr = GLX_TRANSPARENT_TYPE;
                if (!fmt) goto pix_error;
                hTest = pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, curGLXAttr, &tmp);
                if (hTest) goto get_error;
                piValues[i] = GL_FALSE;
                if (GLX_NONE != tmp) piValues[i] = GL_TRUE;
                continue;

            case WGL_PIXEL_TYPE_ARB:
                curGLXAttr = GLX_RENDER_TYPE;
                if (!fmt) goto pix_error;
                hTest = pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, curGLXAttr, &tmp);
                if (hTest) goto get_error;
                TRACE("WGL_PIXEL_TYPE_ARB: GLX_RENDER_TYPE = 0x%x\n", tmp);
                if      (tmp & GLX_RGBA_BIT)           { piValues[i] = WGL_TYPE_RGBA_ARB; }
                else if (tmp & GLX_COLOR_INDEX_BIT)    { piValues[i] = WGL_TYPE_COLORINDEX_ARB; }
                else if (tmp & GLX_RGBA_FLOAT_BIT)     { piValues[i] = WGL_TYPE_RGBA_FLOAT_ATI; }
                else if (tmp & GLX_RGBA_FLOAT_ATI_BIT) { piValues[i] = WGL_TYPE_RGBA_FLOAT_ATI; }
                else if (tmp & GLX_RGBA_UNSIGNED_FLOAT_BIT_EXT) { piValues[i] = WGL_TYPE_RGBA_UNSIGNED_FLOAT_EXT; }
                else {
                    ERR("unexpected RenderType(%x)\n", tmp);
                    piValues[i] = WGL_TYPE_RGBA_ARB;
                }
                continue;

            case WGL_COLOR_BITS_ARB:
                curGLXAttr = GLX_BUFFER_SIZE;
                break;

            case WGL_BIND_TO_TEXTURE_RGB_ARB:
            case WGL_BIND_TO_TEXTURE_RGBA_ARB:
                if (!use_render_texture_emulation) {
                    piValues[i] = GL_FALSE;
                    continue;	
                }
                curGLXAttr = GLX_RENDER_TYPE;
                if (!fmt) goto pix_error;
                hTest = pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, curGLXAttr, &tmp);
                if (hTest) goto get_error;
                if (GLX_COLOR_INDEX_BIT == tmp) {
                    piValues[i] = GL_FALSE;  
                    continue;
                }
                hTest = pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_DRAWABLE_TYPE, &tmp);
                if (hTest) goto get_error;
                piValues[i] = (tmp & GLX_PBUFFER_BIT) ? GL_TRUE : GL_FALSE;
                continue;

            case WGL_BLUE_BITS_ARB:
                curGLXAttr = GLX_BLUE_SIZE;
                break;
            case WGL_RED_BITS_ARB:
                curGLXAttr = GLX_RED_SIZE;
                break;
            case WGL_GREEN_BITS_ARB:
                curGLXAttr = GLX_GREEN_SIZE;
                break;
            case WGL_ALPHA_BITS_ARB:
                curGLXAttr = GLX_ALPHA_SIZE;
                break;
            case WGL_DEPTH_BITS_ARB:
                curGLXAttr = GLX_DEPTH_SIZE;
                break;
            case WGL_STENCIL_BITS_ARB:
                curGLXAttr = GLX_STENCIL_SIZE;
                break;
            case WGL_DOUBLE_BUFFER_ARB:
                curGLXAttr = GLX_DOUBLEBUFFER;
                break;
            case WGL_STEREO_ARB:
                curGLXAttr = GLX_STEREO;
                break;
            case WGL_AUX_BUFFERS_ARB:
                curGLXAttr = GLX_AUX_BUFFERS;
                break;

            case WGL_SUPPORT_GDI_ARB:
                if (!fmt) goto pix_error;
                piValues[i] = (fmt->dwFlags & PFD_SUPPORT_GDI) != 0;
                continue;

            case WGL_DRAW_TO_BITMAP_ARB:
                if (!fmt) goto pix_error;
                piValues[i] = (fmt->dwFlags & PFD_DRAW_TO_BITMAP) != 0;
                continue;

            case WGL_DRAW_TO_WINDOW_ARB:
            case WGL_DRAW_TO_PBUFFER_ARB:
                if (!fmt) goto pix_error;
                hTest = pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_DRAWABLE_TYPE, &tmp);
                if (hTest) goto get_error;
                if((curWGLAttr == WGL_DRAW_TO_WINDOW_ARB && (tmp&GLX_WINDOW_BIT)) ||
                   (curWGLAttr == WGL_DRAW_TO_PBUFFER_ARB && (tmp&GLX_PBUFFER_BIT)))
                    piValues[i] = GL_TRUE;
                else
                    piValues[i] = GL_FALSE;
                continue;

            case WGL_SWAP_METHOD_ARB:
                if (has_swap_method)
                {
                    hTest = pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_SWAP_METHOD_OML, &tmp);
                    if (hTest) goto get_error;
                    switch (tmp)
                    {
                    case GLX_SWAP_EXCHANGE_OML:
                        piValues[i] = WGL_SWAP_EXCHANGE_ARB;
                        break;
                    case GLX_SWAP_COPY_OML:
                        piValues[i] = WGL_SWAP_COPY_ARB;
                        break;
                    case GLX_SWAP_UNDEFINED_OML:
                        piValues[i] = WGL_SWAP_UNDEFINED_ARB;
                        break;
                    default:
                        ERR("Unexpected swap method %x.\n", tmp);
                    }
                }
                else
                {
                    WARN("GLX_OML_swap_method not supported, returning WGL_SWAP_EXCHANGE_ARB.\n");
                    piValues[i] = WGL_SWAP_EXCHANGE_ARB;
                }
                continue;

            case WGL_PBUFFER_LARGEST_ARB:
                curGLXAttr = GLX_LARGEST_PBUFFER;
                break;

            case WGL_SAMPLE_BUFFERS_ARB:
                curGLXAttr = GLX_SAMPLE_BUFFERS_ARB;
                break;

            case WGL_SAMPLES_ARB:
                curGLXAttr = GLX_SAMPLES_ARB;
                break;

            case WGL_FLOAT_COMPONENTS_NV:
                curGLXAttr = GLX_FLOAT_COMPONENTS_NV;
                break;

            case WGL_FRAMEBUFFER_SRGB_CAPABLE_EXT:
                curGLXAttr = GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT;
                break;

            case WGL_TYPE_RGBA_UNSIGNED_FLOAT_EXT:
                curGLXAttr = GLX_RGBA_UNSIGNED_FLOAT_TYPE_EXT;
                break;

            case WGL_ACCUM_RED_BITS_ARB:
                curGLXAttr = GLX_ACCUM_RED_SIZE;
                break;
            case WGL_ACCUM_GREEN_BITS_ARB:
                curGLXAttr = GLX_ACCUM_GREEN_SIZE;
                break;
            case WGL_ACCUM_BLUE_BITS_ARB:
                curGLXAttr = GLX_ACCUM_BLUE_SIZE;
                break;
            case WGL_ACCUM_ALPHA_BITS_ARB:
                curGLXAttr = GLX_ACCUM_ALPHA_SIZE;
                break;
            case WGL_ACCUM_BITS_ARB:
                if (!fmt) goto pix_error;
                hTest = pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_ACCUM_RED_SIZE, &tmp);
                if (hTest) goto get_error;
                piValues[i] = tmp;
                hTest = pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_ACCUM_GREEN_SIZE, &tmp);
                if (hTest) goto get_error;
                piValues[i] += tmp;
                hTest = pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_ACCUM_BLUE_SIZE, &tmp);
                if (hTest) goto get_error;
                piValues[i] += tmp;
                hTest = pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, GLX_ACCUM_ALPHA_SIZE, &tmp);
                if (hTest) goto get_error;
                piValues[i] += tmp;
                continue;

            default:
                FIXME("unsupported %x WGL Attribute\n", curWGLAttr);
        }

        /* Retrieve a GLX FBConfigAttrib when the attribute to query is valid and
         * iPixelFormat != 0. When iPixelFormat is 0 the only value which makes
         * sense to query is WGL_NUMBER_PIXEL_FORMATS_ARB.
         *
         * TODO: properly test the behavior of wglGetPixelFormatAttrib*v on Windows
         *       and check which options can work using iPixelFormat=0 and which not.
         *       A problem would be that this function is an extension. This would
         *       mean that the behavior could differ between different vendors (ATI, Nvidia, ..).
         */
        if (0 != curGLXAttr && iPixelFormat != 0) {
            if (!fmt) goto pix_error;
            hTest = pglXGetFBConfigAttrib(gdi_display, fmt->config.fbc, curGLXAttr, piValues + i);
            if (hTest) goto get_error;
            curGLXAttr = 0;
        } else { 
            piValues[i] = GL_FALSE; 
        }
    }
    return GL_TRUE;

get_error:
    ERR("(%p): unexpected failure on GetFBConfigAttrib(%x) returns FALSE\n", hdc, curGLXAttr);
    return GL_FALSE;

pix_error:
    ERR("(%p): unexpected iPixelFormat(%d) vs nFormats(%d), returns FALSE\n", hdc, iPixelFormat, nb_pixel_formats);
    return GL_FALSE;
}

/**
 * X11DRV_wglGetPixelFormatAttribfvARB
 *
 * WGL_ARB_pixel_format: wglGetPixelFormatAttribfvARB
 */
static BOOL X11DRV_wglGetPixelFormatAttribfvARB( HDC hdc, int iPixelFormat, int iLayerPlane,
                                                 UINT nAttributes, const int *piAttributes, FLOAT *pfValues )
{
    int *attr;
    int ret;
    UINT i;

    TRACE("(%p, %d, %d, %d, %p, %p)\n", hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, pfValues);

    /* Allocate a temporary array to store integer values */
    attr = HeapAlloc(GetProcessHeap(), 0, nAttributes * sizeof(int));
    if (!attr) {
        ERR("couldn't allocate %d array\n", nAttributes);
        return GL_FALSE;
    }

    /* Piggy-back on wglGetPixelFormatAttribivARB */
    ret = X11DRV_wglGetPixelFormatAttribivARB(hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, attr);
    if (ret) {
        /* Convert integer values to float. Should also check for attributes
           that can give decimal values here */
        for (i=0; i<nAttributes;i++) {
            pfValues[i] = attr[i];
        }
    }

    HeapFree(GetProcessHeap(), 0, attr);
    return ret;
}

/**
 * X11DRV_wglBindTexImageARB
 *
 * WGL_ARB_render_texture: wglBindTexImageARB
 */
static BOOL X11DRV_wglBindTexImageARB( struct wgl_pbuffer *object, int iBuffer )
{
    GLboolean ret = GL_FALSE;

    TRACE("(%p, %d)\n", object, iBuffer);

    if (!object->use_render_texture) {
        SetLastError(ERROR_INVALID_HANDLE);
        return GL_FALSE;
    }

    if (use_render_texture_emulation) {
        static BOOL initialized = FALSE;
        int prev_binded_texture = 0;
        GLXContext prev_context;
        GLXDrawable prev_drawable;

        prev_context = pglXGetCurrentContext();
        prev_drawable = pglXGetCurrentDrawable();

        /* Our render_texture emulation is basic and lacks some features (1D/Cube support).
           This is mostly due to lack of demos/games using them. Further the use of glReadPixels
           isn't ideal performance wise but I wasn't able to get other ways working.
        */
        if(!initialized) {
            initialized = TRUE; /* Only show the FIXME once for performance reasons */
            FIXME("partial stub!\n");
        }

        TRACE("drawable=%lx, context=%p\n", object->pbuffer.drawable, prev_context);
        if (!object->tmp_context || object->prev_context != prev_context)
        {
            if (object->tmp_context)
                pglXDestroyContext(gdi_display, object->tmp_context);
            object->tmp_context = pglXCreateNewContext(gdi_display, object->fmt->config.fbc,
                    object->fmt->render_type, prev_context, True);
            object->prev_context = prev_context;
        }

        opengl_funcs.gl.p_glGetIntegerv(object->texture_bind_target, &prev_binded_texture);

        /* Switch to our pbuffer */
        pglXMakeCurrent(gdi_display, object->pbuffer.drawable, object->tmp_context);

        /* Make sure that the prev_binded_texture is set as the current texture state isn't shared between contexts.
         * After that copy the pbuffer texture data. */
        opengl_funcs.gl.p_glBindTexture(object->texture_target, prev_binded_texture);
        opengl_funcs.gl.p_glCopyTexImage2D(object->texture_target, 0, object->use_render_texture, 0, 0, object->width, object->height, 0);

        /* Switch back to the original drawable and context */
        pglXMakeCurrent(gdi_display, prev_drawable, prev_context);
        return GL_TRUE;
    }

    return ret;
}

/**
 * X11DRV_wglReleaseTexImageARB
 *
 * WGL_ARB_render_texture: wglReleaseTexImageARB
 */
static BOOL X11DRV_wglReleaseTexImageARB( struct wgl_pbuffer *object, int iBuffer )
{
    GLboolean ret = GL_FALSE;

    TRACE("(%p, %d)\n", object, iBuffer);

    if (!object->use_render_texture) {
        SetLastError(ERROR_INVALID_HANDLE);
        return GL_FALSE;
    }
    if (use_render_texture_emulation) {
        return GL_TRUE;
    }
    return ret;
}

/**
 * X11DRV_wglGetExtensionsStringEXT
 *
 * WGL_EXT_extensions_string: wglGetExtensionsStringEXT
 */
static const char *X11DRV_wglGetExtensionsStringEXT(void)
{
    TRACE("() returning \"%s\"\n", wglExtensions);
    return wglExtensions;
}

/**
 * X11DRV_wglGetSwapIntervalEXT
 *
 * WGL_EXT_swap_control: wglGetSwapIntervalEXT
 */
static int X11DRV_wglGetSwapIntervalEXT(void)
{
    struct wgl_context *ctx = NtCurrentTeb()->glContext;
    struct gl_drawable *gl;
    int swap_interval;

    TRACE("()\n");

    if (!(gl = get_gl_drawable( WindowFromDC( ctx->hdc ), ctx->hdc )))
    {
        /* This can't happen because a current WGL context is required to get
         * here. Likely the application is buggy.
         */
        WARN("No GL drawable found, returning swap interval 0\n");
        return 0;
    }

    swap_interval = gl->swap_interval;
    release_gl_drawable(gl);

    return swap_interval;
}

/**
 * X11DRV_wglSwapIntervalEXT
 *
 * WGL_EXT_swap_control: wglSwapIntervalEXT
 */
static BOOL X11DRV_wglSwapIntervalEXT(int interval)
{
    struct wgl_context *ctx = NtCurrentTeb()->glContext;
    struct gl_drawable *gl;
    BOOL ret;

    TRACE("(%d)\n", interval);

    /* Without WGL/GLX_EXT_swap_control_tear a negative interval
     * is invalid.
     */
    if (interval < 0 && !has_swap_control_tear)
    {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }

    if (!(gl = get_gl_drawable( WindowFromDC( ctx->hdc ), ctx->hdc )))
    {
        SetLastError(ERROR_DC_NOT_FOUND);
        return FALSE;
    }

    EnterCriticalSection( &context_section );
    ret = set_swap_interval(gl->drawable.d, interval);
    gl->refresh_swap_interval = FALSE;
    if (ret)
        gl->swap_interval = interval;
    else
        SetLastError(ERROR_DC_NOT_FOUND);

    LeaveCriticalSection( &context_section );
    release_gl_drawable(gl);

    return ret;
}

/**
 * X11DRV_wglSetPixelFormatWINE
 *
 * WGL_WINE_pixel_format_passthrough: wglSetPixelFormatWINE
 * This is a WINE-specific wglSetPixelFormat which can set the pixel format multiple times.
 */
static BOOL X11DRV_wglSetPixelFormatWINE(HDC hdc, int format)
{
    return set_pixel_format(hdc, format, TRUE);
}

static BOOL X11DRV_wglQueryCurrentRendererIntegerWINE( GLenum attribute, GLuint *value )
{
    return pglXQueryCurrentRendererIntegerMESA( attribute, value );
}

static const char *X11DRV_wglQueryCurrentRendererStringWINE( GLenum attribute )
{
    return pglXQueryCurrentRendererStringMESA( attribute );
}

static BOOL X11DRV_wglQueryRendererIntegerWINE( HDC dc, GLint renderer, GLenum attribute, GLuint *value )
{
    return pglXQueryRendererIntegerMESA( gdi_display, DefaultScreen(gdi_display), renderer, attribute, value );
}

static const char *X11DRV_wglQueryRendererStringWINE( HDC dc, GLint renderer, GLenum attribute )
{
    return pglXQueryRendererStringMESA( gdi_display, DefaultScreen(gdi_display), renderer, attribute );
}

/**
 * glxRequireVersion (internal)
 *
 * Check if the supported GLX version matches requiredVersion.
 */
static BOOL glxRequireVersion(int requiredVersion)
{
    /* Both requiredVersion and glXVersion[1] contains the minor GLX version */
    return (requiredVersion <= glxVersion[1]);
}

static void register_extension(const char *ext)
{
    if (wglExtensions[0])
        strcat(wglExtensions, " ");
    strcat(wglExtensions, ext);

    TRACE("'%s'\n", ext);
}

/**
 * X11DRV_WineGL_LoadExtensions
 */
static void X11DRV_WineGL_LoadExtensions(void)
{
    wglExtensions[0] = 0;

    /* ARB Extensions */

    if (has_extension( glxExtensions, "GLX_ARB_create_context"))
    {
        register_extension( "WGL_ARB_create_context" );
        opengl_funcs.ext.p_wglCreateContextAttribsARB = X11DRV_wglCreateContextAttribsARB;

        if (has_extension( glxExtensions, "GLX_ARB_create_context_no_error" ))
            register_extension( "WGL_ARB_create_context_no_error" );
        if (has_extension( glxExtensions, "GLX_ARB_create_context_profile"))
            register_extension("WGL_ARB_create_context_profile");
    }

    if (has_extension( glxExtensions, "GLX_ARB_fbconfig_float"))
    {
        register_extension("WGL_ARB_pixel_format_float");
        register_extension("WGL_ATI_pixel_format_float");
    }

    register_extension( "WGL_ARB_extensions_string" );
    opengl_funcs.ext.p_wglGetExtensionsStringARB = X11DRV_wglGetExtensionsStringARB;

    if (glxRequireVersion(3))
    {
        register_extension( "WGL_ARB_make_current_read" );
        opengl_funcs.ext.p_wglGetCurrentReadDCARB   = (void *)1;  /* never called */
        opengl_funcs.ext.p_wglMakeContextCurrentARB = X11DRV_wglMakeContextCurrentARB;
    }

    if (has_extension( glxExtensions, "GLX_ARB_multisample")) register_extension( "WGL_ARB_multisample" );

    if (glxRequireVersion(3))
    {
        register_extension( "WGL_ARB_pbuffer" );
        opengl_funcs.ext.p_wglCreatePbufferARB    = X11DRV_wglCreatePbufferARB;
        opengl_funcs.ext.p_wglDestroyPbufferARB   = X11DRV_wglDestroyPbufferARB;
        opengl_funcs.ext.p_wglGetPbufferDCARB     = X11DRV_wglGetPbufferDCARB;
        opengl_funcs.ext.p_wglQueryPbufferARB     = X11DRV_wglQueryPbufferARB;
        opengl_funcs.ext.p_wglReleasePbufferDCARB = X11DRV_wglReleasePbufferDCARB;
        opengl_funcs.ext.p_wglSetPbufferAttribARB = X11DRV_wglSetPbufferAttribARB;
    }

    register_extension( "WGL_ARB_pixel_format" );
    opengl_funcs.ext.p_wglChoosePixelFormatARB      = X11DRV_wglChoosePixelFormatARB;
    opengl_funcs.ext.p_wglGetPixelFormatAttribfvARB = X11DRV_wglGetPixelFormatAttribfvARB;
    opengl_funcs.ext.p_wglGetPixelFormatAttribivARB = X11DRV_wglGetPixelFormatAttribivARB;

    /* Support WGL_ARB_render_texture when there's support or pbuffer based emulation */
    if (has_extension( glxExtensions, "GLX_ARB_render_texture") ||
        (glxRequireVersion(3) && use_render_texture_emulation))
    {
        register_extension( "WGL_ARB_render_texture" );
        opengl_funcs.ext.p_wglBindTexImageARB    = X11DRV_wglBindTexImageARB;
        opengl_funcs.ext.p_wglReleaseTexImageARB = X11DRV_wglReleaseTexImageARB;

        /* The WGL version of GLX_NV_float_buffer requires render_texture */
        if (has_extension( glxExtensions, "GLX_NV_float_buffer"))
            register_extension("WGL_NV_float_buffer");

        /* Again there's no GLX equivalent for this extension, so depend on the required GL extension */
        if (has_extension(glExtensions, "GL_NV_texture_rectangle"))
            register_extension("WGL_NV_render_texture_rectangle");
    }

    /* EXT Extensions */

    register_extension( "WGL_EXT_extensions_string" );
    opengl_funcs.ext.p_wglGetExtensionsStringEXT = X11DRV_wglGetExtensionsStringEXT;

    /* Load this extension even when it isn't backed by a GLX extension because it is has been around for ages.
     * Games like Call of Duty and K.O.T.O.R. rely on it. Further our emulation is good enough. */
    register_extension( "WGL_EXT_swap_control" );
    opengl_funcs.ext.p_wglSwapIntervalEXT = X11DRV_wglSwapIntervalEXT;
    opengl_funcs.ext.p_wglGetSwapIntervalEXT = X11DRV_wglGetSwapIntervalEXT;

    if (has_extension( glxExtensions, "GLX_EXT_framebuffer_sRGB"))
        register_extension("WGL_EXT_framebuffer_sRGB");

    if (has_extension( glxExtensions, "GLX_EXT_fbconfig_packed_float"))
        register_extension("WGL_EXT_pixel_format_packed_float");

    if (has_extension( glxExtensions, "GLX_EXT_swap_control"))
    {
        swap_control_method = GLX_SWAP_CONTROL_EXT;
        if (has_extension( glxExtensions, "GLX_EXT_swap_control_tear"))
        {
            register_extension("WGL_EXT_swap_control_tear");
            has_swap_control_tear = TRUE;
        }
    }
    else if (has_extension( glxExtensions, "GLX_MESA_swap_control"))
    {
        swap_control_method = GLX_SWAP_CONTROL_MESA;
    }
    else if (has_extension( glxExtensions, "GLX_SGI_swap_control"))
    {
        swap_control_method = GLX_SWAP_CONTROL_SGI;
    }

    /* The OpenGL extension GL_NV_vertex_array_range adds wgl/glX functions which aren't exported as 'real' wgl/glX extensions. */
    if (has_extension(glExtensions, "GL_NV_vertex_array_range"))
    {
        register_extension( "WGL_NV_vertex_array_range" );
        opengl_funcs.ext.p_wglAllocateMemoryNV = pglXAllocateMemoryNV;
        opengl_funcs.ext.p_wglFreeMemoryNV = pglXFreeMemoryNV;
    }

    if (has_extension(glxExtensions, "GLX_OML_swap_method"))
        has_swap_method = TRUE;

    /* WINE-specific WGL Extensions */

    /* In WineD3D we need the ability to set the pixel format more than once (e.g. after a device reset).
     * The default wglSetPixelFormat doesn't allow this, so add our own which allows it.
     */
    register_extension( "WGL_WINE_pixel_format_passthrough" );
    opengl_funcs.ext.p_wglSetPixelFormatWINE = X11DRV_wglSetPixelFormatWINE;

    if (has_extension( glxExtensions, "GLX_MESA_query_renderer" ))
    {
        register_extension( "WGL_WINE_query_renderer" );
        opengl_funcs.ext.p_wglQueryCurrentRendererIntegerWINE = X11DRV_wglQueryCurrentRendererIntegerWINE;
        opengl_funcs.ext.p_wglQueryCurrentRendererStringWINE = X11DRV_wglQueryCurrentRendererStringWINE;
        opengl_funcs.ext.p_wglQueryRendererIntegerWINE = X11DRV_wglQueryRendererIntegerWINE;
        opengl_funcs.ext.p_wglQueryRendererStringWINE = X11DRV_wglQueryRendererStringWINE;
    }
}


/**
 * glxdrv_SwapBuffers
 *
 * Swap the buffers of this DC
 */
static BOOL glxdrv_wglSwapBuffers( HDC hdc )
{
    struct x11drv_escape_flush_gl_drawable escape;
    struct gl_drawable *gl;
    struct wgl_context *ctx = NtCurrentTeb()->glContext;
    INT64 ust, msc, sbc, target_sbc = 0;

    TRACE("(%p)\n", hdc);

    escape.code = X11DRV_FLUSH_GL_DRAWABLE;
    escape.gl_drawable = 0;
    escape.flush = !pglXWaitForSbcOML;

    if (!(gl = get_gl_drawable( WindowFromDC( hdc ), hdc )))
    {
        SetLastError( ERROR_INVALID_HANDLE );
        return FALSE;
    }

    EnterCriticalSection( &context_section );
    if (gl->refresh_swap_interval)
    {
        set_swap_interval(gl->drawable.d, gl->swap_interval);
        gl->refresh_swap_interval = FALSE;
    }
    LeaveCriticalSection( &context_section );

    switch (gl->type)
    {
    case DC_GL_PIXMAP_WIN:
        if (ctx) sync_context( ctx );
        escape.gl_drawable = gl->pixmap;
        if (pglXCopySubBufferMESA) {
            /* (glX)SwapBuffers has an implicit glFlush effect, however
             * GLX_MESA_copy_sub_buffer doesn't. Make sure GL is flushed before
             * copying */
            pglFlush();
            pglXCopySubBufferMESA( gdi_display, gl->drawable.d, 0, 0,
                                   gl->pixmap_size.cx, gl->pixmap_size.cy );
            break;
        }
        if (pglXSwapBuffersMscOML)
        {
            pglFlush();
            target_sbc = pglXSwapBuffersMscOML( gdi_display, gl->drawable.d, 0, 0, 0 );
            break;
        }
        pglXSwapBuffers(gdi_display, gl->drawable.d);
        break;
    case DC_GL_CHILD_WIN:
        if (ctx) sync_context( ctx );
        escape.gl_drawable = gl->window;
        /* fall through */
    default:
        if (escape.gl_drawable && pglXSwapBuffersMscOML)
        {
            pglFlush();
            target_sbc = pglXSwapBuffersMscOML( gdi_display, gl->drawable.d, 0, 0, 0 );
            break;
        }
        pglXSwapBuffers(gdi_display, gl->drawable.d);
        break;
    }

    if (escape.gl_drawable)
    {
        if (pglXWaitForSbcOML)
            pglXWaitForSbcOML( gdi_display, gl->drawable.d, target_sbc, &ust, &msc, &sbc );
        else if (pglXWaitGL)
            pglXWaitGL();
    }
    release_gl_drawable( gl );

    if (escape.gl_drawable) ExtEscape( ctx->hdc, X11DRV_ESCAPE, sizeof(escape), (LPSTR)&escape, 0, NULL );
    return TRUE;
}

static struct opengl_funcs opengl_funcs =
{
    {
        glxdrv_wglCopyContext,              /* p_wglCopyContext */
        glxdrv_wglCreateContext,            /* p_wglCreateContext */
        glxdrv_wglDeleteContext,            /* p_wglDeleteContext */
        glxdrv_wglDescribePixelFormat,      /* p_wglDescribePixelFormat */
        glxdrv_wglGetPixelFormat,           /* p_wglGetPixelFormat */
        glxdrv_wglGetProcAddress,           /* p_wglGetProcAddress */
        glxdrv_wglMakeCurrent,              /* p_wglMakeCurrent */
        glxdrv_wglSetPixelFormat,           /* p_wglSetPixelFormat */
        glxdrv_wglShareLists,               /* p_wglShareLists */
        glxdrv_wglSwapBuffers,              /* p_wglSwapBuffers */
    }
};

#ifdef SONAME_LIBEGL

static XVisualInfo *get_visual_info(EGLConfig config, int *out_fmt_id)
{
    int fmt_id, visinfo_count;
    XVisualInfo visinfo_template;

    peglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &fmt_id);
    visinfo_template.visualid = fmt_id;
    if (out_fmt_id)
        *out_fmt_id = fmt_id;
    return XGetVisualInfo(gdi_display, VisualIDMask, &visinfo_template, &visinfo_count);
}

static BOOL egl_WineGL_InitOpenglInfo(void)
{
    Window win = 0, root = 0;
    const char *gl_renderer;
    const char *gl_version;
    const char* str;
    EGLConfig config;
    EGLint major, minor, configs_count;
    EGLint attribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};
    EGLSurface surface;
    EGLBoolean r;
    XVisualInfo *vis = NULL;
    GLXContext ctx = NULL;
    XSetWindowAttributes attr;
    BOOL ret = FALSE;

    attr.override_redirect = True;
    attr.colormap = None;
    attr.border_pixel = 0;

    display = peglGetDisplay(EGL_DEFAULT_DISPLAY);

    peglInitialize(display, &major, &minor);
    TRACE("Initializing EGL %d.%d on default display.\n", major, minor);
    r = peglChooseConfig(display, attribs, &config, 1, &configs_count);
    peglBindAPI(EGL_OPENGL_API);
    if (r) {
#ifdef __i386__
        WORD old_fs = wine_get_fs();
        /* Create an OpenGL context. Without one we can't query GL information */
        ctx = peglCreateContext(display, config, EGL_NO_CONTEXT, NULL);
        if (wine_get_fs() != old_fs)
        {
            wine_set_fs( old_fs );
            ERR( "%%fs register corrupted, probably broken ATI driver, disabling OpenGL.\n" );
            ERR( "You need to set the \"UseFastTls\" option to \"2\" in your X config file.\n" );
            goto done;
        }
#else
        ctx = peglCreateContext(display, config, EGL_NO_CONTEXT, NULL);
#endif
    }
    if (!ctx) goto done;

    vis = get_visual_info(config, NULL);
    root = RootWindow(gdi_display, vis->screen);
    if (vis->visual != DefaultVisual(gdi_display, vis->screen))
    {
        TRACE("Creating X colormap.\n");
        attr.colormap = XCreateColormap(gdi_display, root, vis->visual, AllocNone);
    }
    if ((win = XCreateWindow(gdi_display, root, -1, -1, 1, 1, 0, vis->depth, InputOutput,
            vis->visual, CWBorderPixel | CWOverrideRedirect | CWColormap, &attr)))
    {
        TRACE("Created X window %lx.\n", win);
        XMapWindow(gdi_display, win);
    }
    else
    {
        TRACE("Using X root window.\n");
        win = root;
    }

    XFlush(gdi_display);

    surface = peglCreateWindowSurface(display, config, win, NULL);
    if(!peglMakeCurrent(display, surface, surface, ctx))
    {
        ERR_(winediag)( "Unable to activate OpenGL context, most likely your OpenGL drivers haven't been installed correctly\n" );
        goto done;
    }

    gl_renderer = (const char *)egl_funcs.gl.p_glGetString(GL_RENDERER);
    gl_version = (const char *) egl_funcs.gl.p_glGetString(GL_VERSION);
    str = (const char *) egl_funcs.gl.p_glGetString(GL_EXTENSIONS);
    glExtensions = HeapAlloc(GetProcessHeap(), 0, strlen(str)+1);
    strcpy(glExtensions, str);

    glxVersion[0] = major;
    glxVersion[1] = minor;
    glxExtensions = peglQueryString(display, EGL_EXTENSIONS);

    TRACE("GL version             : %s.\n", gl_version);
    TRACE("GL renderer            : %s.\n", gl_renderer);
    TRACE("EGL version            : %d.%d.\n", glxVersion[0], glxVersion[1]);
    TRACE("EGL version string     : %s.\n", peglQueryString(display, EGL_VERSION));
    TRACE("EGL vendor             : %s.\n", peglQueryString(display, EGL_VENDOR));

    /* Depending on the cause of software rendering a different rendering string is shown. In case Mesa fails
     * to load a DRI module 'Software Rasterizer' is returned. When Mesa is compiled as a OpenGL reference driver
     * it shows 'Mesa X11'.
     */
    if(!strcmp(gl_renderer, "Software Rasterizer") || !strcmp(gl_renderer, "Mesa X11"))
        ERR_(winediag)("The Mesa OpenGL driver is using software rendering, most likely your OpenGL "
                       "drivers haven't been installed correctly (using GL renderer %s, version %s).\n",
                       debugstr_a(gl_renderer), debugstr_a(gl_version));
    ret = TRUE;

done:
    if (vis)
        XFree(vis);
    if (ctx)
    {
        peglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        peglDestroyContext(display, ctx);
    }
    if (win != root)
        XDestroyWindow(gdi_display, win);
    if (attr.colormap)
        XFreeColormap(gdi_display, attr.colormap);
    if (!ret)
        WARN("couldn't initialize EGL\n");
    return ret;
}

static void egl_init_pixel_formats(void);
static void egl_WineGL_LoadExtensions(void);

static BOOL has_egl(void)
{
    static int init_done;
    static void *opengl_handle = NULL;
    static void *egl_handle = NULL;

    char buffer[200];
    unsigned int i;

    if (init_done)
        return (egl_handle != NULL);
    init_done = 1;

    if (!use_egl)
        return FALSE;

    /* No need to load any other libraries as according to the ABI, libGL should be self-sufficient
       and include all dependencies */
    egl_handle = wine_dlopen(SONAME_LIBEGL, RTLD_NOW|RTLD_GLOBAL, buffer, sizeof(buffer));
    if (!egl_handle)
    {
        ERR("Failed to load libEGL: %s\n", buffer);
        ERR("OpenGL support is disabled.\n");
        return FALSE;
    }

    opengl_handle = wine_dlopen(SONAME_LIBGL, RTLD_NOW|RTLD_GLOBAL, buffer, sizeof(buffer));
    if (!opengl_handle)
    {
        ERR("Failed to load libGL: %s\n", buffer);
        ERR("OpenGL support is disabled.\n");
        goto failed;
    }
    for (i = 0; i < sizeof(opengl_func_names)/sizeof(opengl_func_names[0]); i++)
    {
        if (!(((void **)&egl_funcs.gl)[i] = wine_dlsym(opengl_handle, opengl_func_names[i], NULL, 0)))
        {
            ERR("%s not found in libGL, disabling OpenGL.\n", opengl_func_names[i]);
            goto failed;
        }
    }

    /* redirect some standard OpenGL functions */
#define REDIRECT(func) \
    do { p##func = egl_funcs.gl.p_##func; egl_funcs.gl.p_##func = w##func; } while(0)
    REDIRECT(glFinish);
    REDIRECT(glFlush);
#undef REDIRECT

#define LOAD_FUNCPTR(f) do if((p##f = wine_dlsym(egl_handle, (const char *)#f, NULL, 0)) == NULL) \
    { \
        ERR("%s not found in libEGL, disabling OpenGL.\n", #f); \
        goto failed; \
    } while(0)

    /* FIXME: sort these by EGL version maybe. */
    LOAD_FUNCPTR(eglGetProcAddress);

    LOAD_FUNCPTR(eglGetError);

    LOAD_FUNCPTR(eglGetDisplay);
    LOAD_FUNCPTR(eglInitialize);
    LOAD_FUNCPTR(eglTerminate);

    LOAD_FUNCPTR(eglQueryString);

    LOAD_FUNCPTR(eglGetConfigs);
    LOAD_FUNCPTR(eglChooseConfig);
    LOAD_FUNCPTR(eglGetConfigAttrib);

    LOAD_FUNCPTR(eglCreateWindowSurface);
    LOAD_FUNCPTR(eglCreatePbufferSurface);
    LOAD_FUNCPTR(eglCreatePixmapSurface);
    LOAD_FUNCPTR(eglDestroySurface);
    LOAD_FUNCPTR(eglQuerySurface);

    LOAD_FUNCPTR(eglBindAPI);
    LOAD_FUNCPTR(eglQueryAPI);

    LOAD_FUNCPTR(eglWaitClient);

    LOAD_FUNCPTR(eglReleaseThread);

    LOAD_FUNCPTR(eglCreatePbufferFromClientBuffer);

    LOAD_FUNCPTR(eglSurfaceAttrib);
    LOAD_FUNCPTR(eglBindTexImage);
    LOAD_FUNCPTR(eglReleaseTexImage);

    LOAD_FUNCPTR(eglSwapInterval);

    LOAD_FUNCPTR(eglCreateContext);
    LOAD_FUNCPTR(eglDestroyContext);
    LOAD_FUNCPTR(eglMakeCurrent);

    LOAD_FUNCPTR(eglGetCurrentContext);
    LOAD_FUNCPTR(eglGetCurrentSurface);
    LOAD_FUNCPTR(eglGetCurrentDisplay);
    LOAD_FUNCPTR(eglQueryContext);

    LOAD_FUNCPTR(eglWaitGL);
    LOAD_FUNCPTR(eglWaitNative);
    LOAD_FUNCPTR(eglSwapBuffers);
    LOAD_FUNCPTR(eglCopyBuffers);
#undef LOAD_FUNCPTR

    if (!egl_WineGL_InitOpenglInfo()) goto failed;

    gl_hwnd_context = XUniqueContext();
    gl_pbuffer_context = XUniqueContext();

    egl_WineGL_LoadExtensions();
    egl_init_pixel_formats();
    return TRUE;

failed:
    if (opengl_handle)
        wine_dlclose(opengl_handle, NULL, 0);
    if (egl_handle)
        wine_dlclose(egl_handle, NULL, 0);
    opengl_handle = NULL;
    egl_handle = NULL;
    return FALSE;
}

static const char *debugstr_eglconfig( EGLConfig config )
{
    int id, visual, surface;

    if (!peglGetConfigAttrib(display, config, EGL_CONFIG_ID, &id))
        return "*** invalid config";
    peglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &visual);
    peglGetConfigAttrib(display, config, EGL_SURFACE_TYPE, &surface);
    return wine_dbg_sprintf("fbconfig %#x native visual id %#x surface type %#x", id, visual, surface);
}

static int wgl_attribs_to_egl(const int* wgl_attribs, int* egl_attribs, struct wgl_pbuffer* pbuf)
{
    int nAttribs = 0; //FIXME: PUSH2 macro hardcodes nAttribs...
    unsigned cur = 0;
    int pop;
    int drawattrib = 0;
    int pixelattrib = EGL_DONT_CARE;

    /* The list of WGL attributes is allowed to be NULL. We don't return here for NULL
     * because we need to do fixups for EGL_SURFACE_TYPE. */
    while (wgl_attribs && wgl_attribs[cur])
    {
        TRACE("%x ", wgl_attribs[cur]);

        switch (wgl_attribs[cur]) {
            case WGL_AUX_BUFFERS_ARB:
                pop = wgl_attribs[++cur];
                TRACE("(WGL_AUX_BUFFERS_ARB) unsupported, skipped\n");
                break;
            case WGL_COLOR_BITS_ARB:
                pop = wgl_attribs[++cur];
                PUSH2(egl_attribs, GLX_BUFFER_SIZE, pop);
                TRACE("(WGL_COLOR_BITS_ARB) -> EGL_BUFFER_SIZE: %d\n", pop);
                break;
            case WGL_RED_BITS_ARB:
                pop = wgl_attribs[++cur];
                PUSH2(egl_attribs, GLX_RED_SIZE, pop);
                TRACE("(WGL_BLUE_BITS_ARB) -> EGL_RED_SIZE: %d\n", pop);
                break;
            case WGL_GREEN_BITS_ARB:
                pop = wgl_attribs[++cur];
                PUSH2(egl_attribs, GLX_GREEN_SIZE, pop);
                TRACE("(WGL_GREEN_BITS_ARB) -> EGL_GREEN_SIZE: %d\n", pop);
                break;
            case WGL_BLUE_BITS_ARB:
                pop = wgl_attribs[++cur];
                PUSH2(egl_attribs, GLX_BLUE_SIZE, pop);
                TRACE("(WGL_BLUE_BITS_ARB) -> EGL_BLUE_SIZE: %d\n", pop);
                break;
            case WGL_ALPHA_BITS_ARB:
                pop = wgl_attribs[++cur];
                PUSH2(egl_attribs, GLX_ALPHA_SIZE, pop);
                TRACE("(WGL_ALPHA_BITS_ARB) -> EGL_ALPHA_SIZE: %d\n", pop);
                break;
            case WGL_DEPTH_BITS_ARB:
                pop = wgl_attribs[++cur];
                PUSH2(egl_attribs, GLX_DEPTH_SIZE, pop);
                TRACE("(WGL_DEPTH_BITS_ARB) -> EGL_DEPTH_SIZE: %d\n", pop);
                break;
            case WGL_STENCIL_BITS_ARB:
                pop = wgl_attribs[++cur];
                PUSH2(egl_attribs, GLX_STENCIL_SIZE, pop);
                TRACE("(WGL_STENCIL_BITS_ARB) -> EGL_STENCIL_SIZE: %d\n", pop);
                break;
            case WGL_DOUBLE_BUFFER_ARB:
                pop = wgl_attribs[++cur];
                TRACE("(WGL_DOUBLE_BUFFER_ARB) unsupported, skipped\n");
                /* FIXME: This could probably be turned into EGL_RENDER_BUFFER (which is an
                 * attribute of eglCreateWindowSurface). */
                break;
            case WGL_STEREO_ARB:
                pop = wgl_attribs[++cur];
                TRACE("(WGL_STEREO_ARB) unsupported, skipped\n");
                break;
            case WGL_PIXEL_TYPE_ARB:
                pop = wgl_attribs[++cur];
                TRACE("(WGL_PIXEL_TYPE_ARB) %d\n", pop);
                switch (pop) {
                    default:
                        FIXME("unsupported PixelType %#x\n", pop);
                        /* Fall through. */
                    case WGL_TYPE_RGBA_ARB:
                        pixelattrib = EGL_RGB_BUFFER;
                        break;
                }
                break;

            case WGL_SUPPORT_GDI_ARB:
                pop = wgl_attribs[++cur];
                PUSH2(egl_attribs, EGL_NATIVE_RENDERABLE, pop);
                TRACE("(WGL_SUPPORT_GDI_ARB) -> EGL_NATIVE_RENDERABLE %d\n", pop);
                break;
            case WGL_DRAW_TO_BITMAP_ARB:
                pop = wgl_attribs[++cur];
                TRACE("(WGL_DRAW_TO_BITMAP_ARB) -> EGL_PIXMAP_BIT %d\n", pop);
                if (pop)
                    drawattrib |= EGL_PIXMAP_BIT;
                break;
            case WGL_DRAW_TO_WINDOW_ARB:
                pop = wgl_attribs[++cur];
                TRACE("(WGL_DRAW_TO_WINDOW_ARB) -> EGL_WINDOW_BIT %d\n", pop);
                if (pop)
                    drawattrib |= EGL_WINDOW_BIT;
                break;
            case WGL_DRAW_TO_PBUFFER_ARB:
                pop = wgl_attribs[++cur];
                TRACE("(WGL_DRAW_TO_PBUFFER_ARB) -> EGL_PBUFFER_BIT %d\n", pop);
                if (pop)
                    drawattrib |= EGL_PBUFFER_BIT;
                break;

            case WGL_ACCELERATION_ARB:
                pop = wgl_attribs[++cur];
                /* FIXME: Could make use of EGL_CONFIG_CAVEAT. */
                TRACE("(WGL_ACCELERATION_ARB) unsupported, skipped\n");
                break;
            case WGL_SUPPORT_OPENGL_ARB:
                pop = wgl_attribs[++cur];
                TRACE("(WGL_SUPPORT_OPENGL_ARB) unsupported, skipped\n");
                break;

            case WGL_SWAP_METHOD_ARB:
                pop = wgl_attribs[++cur];
                /* FIXME: We could use EGL_SWAP_BEHAVIOR_PRESERVED_BIT. */
                TRACE("(WGL_SWAP_METHOD_ARB) %#x unsupported, skipped\n", pop);
                break;

            case WGL_PBUFFER_LARGEST_ARB:
                pop = wgl_attribs[++cur];
                //FIXME: Check if this makes actually some sense
                PUSH2(egl_attribs, EGL_MAX_PBUFFER_PIXELS, pop);
                TRACE("(WGL_PBUFFER_LARGEST_ARB) -> EGL_MAX_PBUFFER_PIXELS %x\n", pop);
                break;

            case WGL_SAMPLE_BUFFERS_ARB:
                pop = wgl_attribs[++cur];
                PUSH2(egl_attribs, EGL_SAMPLE_BUFFERS, pop);
                TRACE("(WGL_SAMPLE_BUFFERS_ARB) -> EGL_SAMPLE_BUFFERS %x\n", pop);
                break;
            case WGL_SAMPLES_ARB:
                pop = wgl_attribs[++cur];
                PUSH2(egl_attribs, EGL_SAMPLES, pop);
                TRACE("(WGL_SAMPLES_ARB) -> EGL_SAMPLES %x\n", pop);
                break;

            case WGL_TEXTURE_FORMAT_ARB:
            case WGL_TEXTURE_TARGET_ARB:
            case WGL_MIPMAP_TEXTURE_ARB:
                TRACE("WGL_render_texture Attributes: %x as %x\n", wgl_attribs[cur], wgl_attribs[cur + 1]);
                pop = wgl_attribs[++cur];
                if (!pbuf)
                    ERR("trying to use GLX_Pbuffer Attributes without Pbuffer (was %x)\n", wgl_attribs[cur]);
                if (!use_render_texture_emulation)
                {
                    if (pop != WGL_NO_TEXTURE_ARB)
                    {
                        ERR("trying to use WGL_render_texture Attributes without support (was %x)\n", wgl_attribs[cur]);
                        return -1; /** error: don't support it */
                    }
                    else
                    {
                        drawattrib |= EGL_PBUFFER_BIT;
                    }
                }
                break;

            case WGL_FLOAT_COMPONENTS_NV:
                ++cur;
                TRACE("(WGL_FLOAT_COMPONENTS_NV) unsupported, skipping\n");
                break;
            case WGL_BIND_TO_TEXTURE_DEPTH_NV:
            case WGL_BIND_TO_TEXTURE_RGB_ARB:
            case WGL_BIND_TO_TEXTURE_RGBA_ARB:
            case WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_R_NV:
            case WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_RG_NV:
            case WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_RGB_NV:
            case WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_RGBA_NV:
                pop = wgl_attribs[++cur];
                TRACE("unsupported, skipping\n");
                break;
            case WGL_FRAMEBUFFER_SRGB_CAPABLE_EXT:
                pop = wgl_attribs[++cur];
                TRACE("(WGL_FRAMEBUFFER_SRGB_CAPABLE_EXT) %#x unsupported, skipping\n", pop);
                break;
            case WGL_TYPE_RGBA_UNSIGNED_FLOAT_EXT:
                pop = wgl_attribs[++cur];
                TRACE("(WGL_RGBA_UNSIGNED_FLOAT_EXT) %#x unsupported, skipping\n", pop);
                break ;
            default:
                FIXME("unsupported WGL attribute %x\n", wgl_attribs[cur]);
                break;
        }
        ++cur;
    }

    if (!drawattrib)
        drawattrib = EGL_DONT_CARE;
    PUSH2(egl_attribs, EGL_SURFACE_TYPE, drawattrib);
    TRACE("EGL_SURFACE_TYPE %#x\n", drawattrib);

    PUSH2(egl_attribs, EGL_COLOR_BUFFER_TYPE, pixelattrib);
    TRACE("EGL_COLOR_BUFFER_TYPE %#x\n", pixelattrib);

    PUSH2(egl_attribs, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT);
    TRACE("EGL_RENDERABLE_TYPE %#x\n", EGL_OPENGL_BIT);

    return nAttribs;
}

static int get_color_buffer_type_from_config(EGLConfig config)
{
    int type;

    peglGetConfigAttrib(display, config, EGL_COLOR_BUFFER_TYPE, &type);
    return type;
}

static BOOL check_config_bitmap_capability(EGLConfig config)
{
    int value;

    /* FIXME: Should also return false for double buffered configs, but... */
    peglGetConfigAttrib(display, config, EGL_NATIVE_RENDERABLE, &value);
    return value;
}

static void egl_init_pixel_formats(void)
{
    struct wgl_pixel_format *list;
    int size = 0, onscreen_size = 0;
    int fmt_id, cfgs_count, i, run, bmp_formats;
    EGLConfig *cfgs;
    XVisualInfo *visinfo;

    peglGetConfigs(display, NULL, 0, &cfgs_count);
    cfgs = HeapAlloc(GetProcessHeap(), 0, cfgs_count * sizeof(*cfgs));
    peglGetConfigs(display, cfgs, cfgs_count, &cfgs_count);
    if (!cfgs || !cfgs_count)
    {
        HeapFree(GetProcessHeap(), 0, cfgs);
        ERR("eglGetConfigs returned no configs.\n");
        return;
    }

    for (i = 0, bmp_formats = 0; i < cfgs_count; i++)
    {
        if (check_config_bitmap_capability(cfgs[i]))
            bmp_formats++;
    }
    TRACE("Found %d bitmap capable configs\n", bmp_formats);

    list = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (cfgs_count + bmp_formats) * sizeof(*list));

    /* Fill the pixel format list. Put onscreen formats at the top and offscreen ones at the bottom. */
    for (run = 0; run < 2; run++)
    {
        for (i = 0; i < cfgs_count; i++)
        {
            visinfo = get_visual_info(cfgs[i], &fmt_id);

            /* The first run we only add onscreen formats (ones which have an associated X Visual).
             * The second run we only set offscreen formats. */
            if (!run && visinfo)
            {
                /* We implement child window rendering using offscreen buffers (using composite or an XPixmap).
                 * The contents is copied to the destination using XCopyArea. For the copying to work
                 * the depth of the source and destination window should be the same. In general this should
                 * not be a problem for OpenGL as drivers only advertise formats with a similar depth (or no depth).
                 * As of the introduction of composition managers at least Nvidia now also offers ARGB visuals
                 * with a depth of 32 in addition to the default 24 bit. In order to prevent BadMatch errors we only
                 * list formats with the same depth. */
                if (visinfo->depth != default_visual.depth)
                {
                    XFree(visinfo);
                    continue;
                }

                if (get_color_buffer_type_from_config(cfgs[i]) == EGL_RGB_BUFFER)
                {
                    TRACE("Found onscreen format CONFIG_ID %#x corresponding to iPixelFormat %d at EGL index %d\n", fmt_id, size+1, i);
                    list[size].config.c = cfgs[i];
                    list[size].fmt_id = fmt_id;
                    //list[size].render_type = get_color_buffer_type_from_config(cfgs[i]);
                    list[size].dwFlags = 0;
                    size++;
                    onscreen_size++;

                    /* Clone a format if it is bitmap capable for indirect rendering to bitmaps */
                    if(check_config_bitmap_capability(cfgs[i]))
                    {
                        TRACE("Found bitmap capable format FBCONFIG_ID %#x corresponding to iPixelFormat %d at GLX index %d\n", fmt_id, size+1, i);
                        list[size].config.c = cfgs[i];
                        list[size].fmt_id = fmt_id;
                        //list[size].render_type = get_render_type_from_config(display, cfgs[i]);
                        list[size].dwFlags = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_GDI | PFD_GENERIC_FORMAT;
                        size++;
                        onscreen_size++;
                    }
                }
            }
            else if (run && !visinfo)
            {
                int window_drawable=0;
                peglGetConfigAttrib(display, cfgs[i], EGL_SURFACE_TYPE, &window_drawable);

                /* Recent Nvidia drivers and DRI drivers offer window drawable formats without a visual.
                 * This are formats like 16-bit rgb on a 24-bit desktop. In order to support these formats
                 * onscreen we would have to use glXCreateWindow instead of XCreateWindow. Further it will
                 * likely make our child window opengl rendering more complicated since likely you can't use
                 * XCopyArea on a GLX Window.
                 * For now ignore fbconfigs which are window drawable but lack a visual. */
                if (window_drawable & EGL_WINDOW_BIT)
                {
                    TRACE("Skipping CONFIG_ID 0x%x as an offscreen format because it is window_drawable\n", fmt_id);
                    continue;
                }

                if (get_color_buffer_type_from_config(cfgs[i]) == EGL_RGB_BUFFER)
                {
                    TRACE("Found offscreen format CONFIG_ID 0x%x corresponding to iPixelFormat %d at GLX index %d\n", fmt_id, size+1, i);
                    list[size].config.c = cfgs[i];
                    list[size].fmt_id = fmt_id;
                    //list[size].render_type = get_render_type_from_config(display, cfgs[i]);
                    list[size].dwFlags = 0;
                    size++;
                }
            }

            if (visinfo) XFree(visinfo);
        }
    }

    HeapFree(GetProcessHeap(), 0, cfgs);

    pixel_formats = list;
    nb_pixel_formats = size;
    nb_onscreen_formats = onscreen_size;
}

/* Given the current context, make sure its drawable is sync'd */
static inline void egl_sync_context(struct wgl_context *context)
{
    if (context->refresh_drawables)
    {
        //eglBindApi();
        peglMakeCurrent(display, context->drawables[0]->drawable.s,
                    context->drawables[1]->drawable.s, context->ctx.egl);
        context->refresh_drawables = FALSE;
    }
}

static EGLContext create_eglcontext(Display *display, struct wgl_context *context, EGLContext shareList)
{
    EGLContext ctx;

    if (context->numAttribs || context->gles)
    {
        if (context->gles)
            peglBindAPI(EGL_OPENGL_ES_API);
        else
            peglBindAPI(EGL_OPENGL_API);
        ctx = peglCreateContext(display, context->fmt->config.c, shareList, context->attribList);
    }
    else
    {
        peglBindAPI(EGL_OPENGL_API);
        ctx = peglCreateContext(display, context->fmt->config.c, shareList, NULL);
    }

    return ctx;
}

/* This has to work for the GLX case too (invoked through destroy_gl_drawable()). */
static void free_egl_drawable(struct gl_drawable *gl)
{
    if (gl->drawable.s)
        peglDestroySurface(display, gl->drawable.s);
    switch (gl->type)
    {
        case DC_GL_CHILD_WIN:
            XDestroyWindow(gdi_display, gl->drawable.d);
            break;
        case DC_GL_PIXMAP_WIN:
            XFreePixmap(gdi_display, gl->pixmap);
            break;
        default:
            break;
    }
    HeapFree(GetProcessHeap(), 0, gl);
}

static struct gl_drawable *create_egl_drawable(HWND hwnd, const struct wgl_pixel_format *format)
{
    struct gl_drawable *gl, *prev;
    XVisualInfo *visual = format->visual;
    RECT rect;
    int width, height;

    GetClientRect( hwnd, &rect );
    width  = min( max( 1, rect.right ), 65535 );
    height = min( max( 1, rect.bottom ), 65535 );

    if (!(gl = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*gl) ))) return NULL;

    /* Default GLX and WGL swap interval is 1, but in case of glXSwapIntervalSGI
     * there is no way to query it, so we have to store it here.
     */
    gl->swap_interval = 1;
    gl->refresh_swap_interval = TRUE;
    gl->format = format;
    gl->ref = 1;

    if (GetAncestor( hwnd, GA_PARENT ) == GetDesktopWindow())  /* top-level window */
    {
        gl->type = DC_GL_WINDOW;
        gl->window = create_client_window( hwnd, visual );
        if (gl->window)
            gl->drawable.s = peglCreateWindowSurface(display, gl->format->config.c, gl->drawable.d, NULL);
        TRACE( "%p created client %lx drawable %lx\n", hwnd, gl->window, gl->drawable.d );
    }
#ifdef SONAME_LIBXCOMPOSITE
    else if(usexcomposite)
    {
        gl->type = DC_GL_CHILD_WIN;
        gl->window = create_client_window( hwnd, visual );
        if (gl->window)
        {
            pXCompositeRedirectWindow( gdi_display, gl->window, CompositeRedirectManual );
            XMapWindow(gdi_display, gl->drawable.d);
            gl->drawable.s = peglCreateWindowSurface(display, gl->format->config.c, gl->drawable.d, NULL);
        }
        TRACE( "%p created child %lx drawable %lx\n", hwnd, gl->window, gl->drawable.d );
    }
#endif
    else
    {
        WARN("XComposite is not available, using GLXPixmap hack\n");

        gl->type = DC_GL_PIXMAP_WIN;
        gl->pixmap = XCreatePixmap( gdi_display, root_window, width, height, visual->depth );
        if (gl->pixmap)
        {
            gl->drawable.s = peglCreatePixmapSurface(display, gl->format->config.c, gl->pixmap, NULL);
            if (!gl->drawable.d)
                XFreePixmap(gdi_display, gl->pixmap);
            gl->pixmap_size.cx = width;
            gl->pixmap_size.cy = height;
        }
    }

    if (!gl->drawable.s)
    {
        HeapFree( GetProcessHeap(), 0, gl );
        return NULL;
    }

    EnterCriticalSection( &context_section );
    if (!XFindContext( gdi_display, (XID)hwnd, gl_hwnd_context, (char **)&prev ))
    {
        gl->swap_interval = prev->swap_interval;
        release_gl_drawable( prev );
    }
    XSaveContext( gdi_display, (XID)hwnd, gl_hwnd_context, (char *)grab_gl_drawable(gl) );
    LeaveCriticalSection( &context_section );
    return gl;
}

//FIXME: This is the same as GLX except for the get_visual_info() call
static BOOL egl_set_win_format(HWND hwnd, const struct wgl_pixel_format *format)
{
    struct gl_drawable *gl;

    if (!format->visual) return FALSE;

    if (!(gl = create_egl_drawable( hwnd, format ))) return FALSE;

    TRACE( "created GL drawable %p for win %p %s\n",
           gl->drawable.s, hwnd, debugstr_eglconfig( format->config.c ));

    XFlush( gdi_display );
    release_gl_drawable( gl );

    __wine_set_pixel_format( hwnd, pixel_format_index( format ));
    return TRUE;
}

//FIXME: mostly the same as the GLX variant, except for the GLX/EGL calls. Merge...
void egl_sync_gl_drawable(HWND hwnd)
{
    struct gl_drawable *old, *new;

    if (!(old = get_gl_drawable( hwnd, 0 ))) return;

    switch (old->type)
    {
        case DC_GL_PIXMAP_WIN:
            if (!(new = create_egl_drawable( hwnd, old->format ))) break;
            mark_drawable_dirty( old, new );
            XFlush( gdi_display );
            TRACE( "Recreated GL drawable %p to replace %p\n", new->drawable.s, old->drawable.s );
            release_gl_drawable( new );
            break;
        default:
            break;
    }
    release_gl_drawable( old );
}
#endif

void set_gl_drawable_parent(HWND hwnd, HWND parent)
{
    struct gl_drawable *old, *new;
#ifdef SONAME_LIBEGL
    BOOL egl = has_egl();
#endif

    if (!(old = get_gl_drawable( hwnd, 0 ))) return;

#ifdef SONAME_LIBEGL
    if (egl)
        TRACE("setting drawable %p parent %p\n", old->drawable.s, parent);
    else
#endif
        TRACE("setting drawable %lx parent %p\n", old->drawable.d, parent);

    switch (old->type)
    {
        case DC_GL_WINDOW:
            break;
        case DC_GL_CHILD_WIN:
        case DC_GL_PIXMAP_WIN:
            if (parent == GetDesktopWindow()) break;
            /* fall through */
        default:
        release_gl_drawable( old );
        return;
    }

#ifdef SONAME_LIBEGL
    if (egl)
        new = create_egl_drawable( hwnd, old->format );
    else
#endif
        new = create_gl_drawable( hwnd, old->format );
    if (new)
    {
        mark_drawable_dirty( old, new );
        release_gl_drawable( new );
    }
    else
    {
        destroy_gl_drawable( hwnd );
        __wine_set_pixel_format( hwnd, 0 );
    }
    release_gl_drawable( old );
}

void destroy_gl_drawable(HWND hwnd)
{
    struct gl_drawable *gl;

    EnterCriticalSection( &context_section );
    if (!XFindContext( gdi_display, (XID)hwnd, gl_hwnd_context, (char **)&gl) )
    {
        XDeleteContext( gdi_display, (XID)hwnd, gl_hwnd_context );
        release_gl_drawable( gl );
    }
    LeaveCriticalSection( &context_section );
}

#ifdef SONAME_LIBEGL
static int egldrv_wglDescribePixelFormat(HDC hdc, int iPixelFormat,
        UINT nBytes, PIXELFORMATDESCRIPTOR *ppfd)
{
    int value;
    int rb,gb,bb,ab;
  const struct wgl_pixel_format *fmt;

  if (!has_egl()) return 0;

  TRACE("(%p,%d,%d,%p)\n", hdc, iPixelFormat, nBytes, ppfd);

  if (!ppfd) return nb_onscreen_formats;

  /* Look for the iPixelFormat in our list of supported formats. If it is supported we get the index in the FBConfig table and the number of supported formats back */
  fmt = get_pixel_format(gdi_display, iPixelFormat, FALSE /* Offscreen */);
  if (!fmt)
  {
      WARN("unexpected format %d\n", iPixelFormat);
      return 0;
  }

  if (nBytes < sizeof(PIXELFORMATDESCRIPTOR)) {
    ERR("Wrong structure size !\n");
    /* Should set error */
    return 0;
  }

  memset(ppfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
  ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
  ppfd->nVersion = 1;

  /* These flags are always the same... */
  ppfd->dwFlags = PFD_SUPPORT_OPENGL;
  /* Now the flags extracted from the Visual */

  peglGetConfigAttrib(display, fmt->config.c, EGL_SURFACE_TYPE, &value);
  if(value & EGL_WINDOW_BIT)
      ppfd->dwFlags |= PFD_DRAW_TO_WINDOW;

  /* On Windows bitmap rendering is only offered using the GDI Software renderer. We reserve some formats (see get_formats for more info)
   * for bitmap rendering since we require indirect rendering for this. Further pixel format logs of a GeforceFX, Geforce8800GT, Radeon HD3400 and a
   * Radeon 9000 indicated that all bitmap formats have PFD_SUPPORT_GDI. Except for 2 formats on the Radeon 9000 none of the hw accelerated formats
   * offered the GDI bit either. */
  ppfd->dwFlags |= fmt->dwFlags & (PFD_DRAW_TO_BITMAP | PFD_SUPPORT_GDI);

  /* PFD_GENERIC_FORMAT - gdi software rendering
   * PFD_GENERIC_ACCELERATED - some parts are accelerated by a display driver (MCD e.g. 3dfx minigl)
   * none set - full hardware accelerated by a ICD
   *
   * We only set PFD_GENERIC_FORMAT on bitmap formats (see get_formats) as that's what ATI and Nvidia Windows drivers do  */
  ppfd->dwFlags |= fmt->dwFlags & (PFD_GENERIC_FORMAT | PFD_GENERIC_ACCELERATED);

  /* Report all formats as double buffered for the time being. */
  /* FIXME: Maybe we could report each format twice, once as double buffered and once single buffered. */
  ppfd->dwFlags |= PFD_DOUBLEBUFFER;
  ppfd->dwFlags &= ~PFD_SUPPORT_GDI;

  //pglXGetFBConfigAttrib(gdi_display, fmt->fbconfig, GLX_STEREO, &value); if (value) ppfd->dwFlags |= PFD_STEREO;

  ppfd->iPixelType = PFD_TYPE_RGBA;

  /* Color bits */
  peglGetConfigAttrib(display, fmt->config.c, EGL_BUFFER_SIZE, &value);
  ppfd->cColorBits = value;

  /* Red, green, blue and alpha bits / shifts */
  peglGetConfigAttrib(display, fmt->config.c, EGL_RED_SIZE, &rb);
  peglGetConfigAttrib(display, fmt->config.c, EGL_GREEN_SIZE, &gb);
  peglGetConfigAttrib(display, fmt->config.c, EGL_BLUE_SIZE, &bb);
  peglGetConfigAttrib(display, fmt->config.c, EGL_ALPHA_SIZE, &ab);

  ppfd->cRedBits = rb;
  ppfd->cRedShift = gb + bb + ab;
  ppfd->cBlueBits = bb;
  ppfd->cBlueShift = ab;
  ppfd->cGreenBits = gb;
  ppfd->cGreenShift = bb + ab;
  ppfd->cAlphaBits = ab;
  ppfd->cAlphaShift = 0;

  /* Depth bits */
  peglGetConfigAttrib(display, fmt->config.c, EGL_DEPTH_SIZE, &value);
  ppfd->cDepthBits = value;

  /* stencil bits */
  peglGetConfigAttrib(display, fmt->config.c, EGL_STENCIL_SIZE, &value);
  ppfd->cStencilBits = value;

  ppfd->iLayerType = PFD_MAIN_PLANE;

  if (TRACE_ON(wgl))
      dump_PIXELFORMATDESCRIPTOR(ppfd);

  return nb_onscreen_formats;
}

//FIXME: This is exactly the same as the GLX one
static int egldrv_wglGetPixelFormat(HDC hdc)
{
    struct gl_drawable *gl;
    int ret = 0;

    if ((gl = get_gl_drawable(WindowFromDC(hdc), hdc)))
    {
        ret = pixel_format_index(gl->format);
        /* Offscreen formats can't be used with traditional WGL calls.
         * As has been verified on Windows GetPixelFormat doesn't fail but returns iPixelFormat=1. */
        if (!is_onscreen_pixel_format(ret)) ret = 1;
        release_gl_drawable(gl);
    }
    TRACE("%p -> %d\n", hdc, ret);
    return ret;
}

static BOOL egldrv_wglSetPixelFormat(HDC hdc, int iPixelFormat, const PIXELFORMATDESCRIPTOR *ppfd)
{
    const struct wgl_pixel_format *fmt;
    int value;
    struct gl_drawable *gl;
    HWND hwnd = WindowFromDC( hdc );

    TRACE("(%p,%d,%p)\n", hdc, iPixelFormat, ppfd);

    if (!hwnd || hwnd == GetDesktopWindow())
    {
        WARN("not a proper window DC %p/%p\n", hdc, hwnd);
        return FALSE;
    }

    /* Check if iPixelFormat is in our list of supported formats to see if it is supported. */
    fmt = get_pixel_format(gdi_display, iPixelFormat, FALSE /* Offscreen */);
    if(!fmt) {
        ERR("Invalid iPixelFormat: %d\n", iPixelFormat);
        return FALSE;
    }

    peglGetConfigAttrib(display, fmt->config.c, EGL_SURFACE_TYPE, &value);
    if (!(value & EGL_WINDOW_BIT))
    {
        WARN("Pixel format %d is not compatible for window rendering\n", iPixelFormat);
        return FALSE;
    }

    if ((gl = get_gl_drawable(hwnd, hdc)))
    {
        int prev = pixel_format_index(gl->format);
        release_gl_drawable(gl);
        return prev == iPixelFormat;  /* cannot change it if already set */
    }

    if (TRACE_ON(wgl))
    {
        TRACE("EGL config %u:\n", fmt->fmt_id);
        peglGetConfigAttrib(display, fmt->config.c, EGL_NATIVE_VISUAL_ID, &value);
        TRACE(" - VISUAL_ID     0x%x\n", value);
        peglGetConfigAttrib(display, fmt->config.c, EGL_SURFACE_TYPE, &value);
        TRACE(" - SURFACE_TYPE 0x%x\n", value);
    }

    return egl_set_win_format( hwnd, fmt );
}

static BOOL egldrv_wglCopyContext(struct wgl_context *src, struct wgl_context *dst, UINT mask)
{
    FIXME("%p -> %p mask %#x unsupported\n", src, dst, mask);

    return FALSE;
}

//FIXME: Same as the GLX version except for get_visual_info().
static struct wgl_context *egldrv_wglCreateContext(HDC hdc)
{
    struct wgl_context *ret;
    struct gl_drawable *gl;

    if (!(gl = get_gl_drawable(WindowFromDC(hdc), hdc)))
    {
        SetLastError(ERROR_INVALID_PIXEL_FORMAT);
        return NULL;
    }

    if ((ret = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*ret))))
    {
        ret->hdc = hdc;
        ret->fmt = gl->format;
        ret->ctx.egl = create_eglcontext(display, ret, NULL);
        EnterCriticalSection( &context_section );
        list_add_head(&context_list, &ret->entry);
        LeaveCriticalSection( &context_section );
    }
    release_gl_drawable(gl);
    TRACE("%p -> %p\n", hdc, ret);
    return ret;
}

static BOOL egldrv_wglDeleteContext(struct wgl_context *ctx)
{
    TRACE("(%p)\n", ctx);

    EnterCriticalSection(&context_section);
    list_remove(&ctx->entry);
    LeaveCriticalSection(&context_section);

    if (ctx->ctx.egl)
        peglDestroyContext(display, ctx->ctx.egl);
    release_gl_drawable( ctx->drawables[0] );
    release_gl_drawable( ctx->drawables[1] );
    release_gl_drawable( ctx->new_drawables[0] );
    release_gl_drawable( ctx->new_drawables[1] );
    return HeapFree(GetProcessHeap(), 0, ctx);
}

static PROC egldrv_wglGetProcAddress(LPCSTR lpszProc)
{
    if (!strncmp(lpszProc, "wgl", 3)) return NULL;
    return peglGetProcAddress(lpszProc);
}

static BOOL egldrv_wglMakeCurrent(HDC hdc, struct wgl_context *ctx)
{
    BOOL ret = FALSE;
    struct gl_drawable *gl;

    TRACE("(%p,%p)\n", hdc, ctx);

    if (!ctx)
    {
        peglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        NtCurrentTeb()->glContext = NULL;
        return TRUE;
    }

    if ((gl = get_gl_drawable(WindowFromDC(hdc), hdc)))
    {
        if (ctx->fmt != gl->format)
        {
            WARN("mismatched pixel format hdc %p %p ctx %p %p\n", hdc, gl->format, ctx, ctx->fmt);
            SetLastError(ERROR_INVALID_PIXEL_FORMAT);
            goto done;
        }

        TRACE("hdc %p EGL surface %p fmt %p ctx %p %s\n", hdc,
              gl->drawable.s, gl->format, ctx->ctx.egl, debugstr_eglconfig(gl->format->config.c));

        EnterCriticalSection( &context_section );
        ret = peglMakeCurrent(display, gl->drawable.s, gl->drawable.s, ctx->ctx.egl);
        if (ret)
        {
            NtCurrentTeb()->glContext = ctx;
            ctx->has_been_current = TRUE;
            ctx->hdc = hdc;
            set_context_drawables( ctx, gl, gl );
            ctx->refresh_drawables = FALSE;
            LeaveCriticalSection( &context_section );
            goto done;
        }
        LeaveCriticalSection( &context_section );
    }
    SetLastError(ERROR_INVALID_HANDLE);

done:
    release_gl_drawable(gl);
    TRACE("%p,%p returning %d\n", hdc, ctx, ret);
    if (!ret)
        TRACE("Last error %#x\n", peglGetError());
    return ret;
}

static BOOL egl_wglMakeContextCurrentARB(HDC draw_hdc, HDC read_hdc, struct wgl_context *ctx)
{
    BOOL ret = FALSE;
    struct gl_drawable *draw_gl, *read_gl = NULL;

    TRACE("(%p,%p,%p)\n", draw_hdc, read_hdc, ctx);

    if (!ctx)
    {
        peglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        NtCurrentTeb()->glContext = NULL;
        return TRUE;
    }

    if ((draw_gl = get_gl_drawable(WindowFromDC(draw_hdc), draw_hdc)))
    {
        read_gl = get_gl_drawable(WindowFromDC(read_hdc), read_hdc);
        EnterCriticalSection( &context_section );
        //FIXME: not sure that EGL_NO_SURFACE makes sense here
        ret = peglMakeCurrent(display, draw_gl->drawable.s,
                read_gl ? read_gl->drawable.s : EGL_NO_SURFACE, ctx->ctx.egl);
        if (ret)
        {
            ctx->has_been_current = TRUE;
            ctx->hdc = draw_hdc;
            set_context_drawables( ctx, draw_gl, read_gl );
            ctx->refresh_drawables = FALSE;
            NtCurrentTeb()->glContext = ctx;
            LeaveCriticalSection( &context_section );
            goto done;
        }
        LeaveCriticalSection( &context_section );
    }
    SetLastError(ERROR_INVALID_HANDLE);
done:
    release_gl_drawable(read_gl);
    release_gl_drawable(draw_gl);
    TRACE( "%p,%p,%p returning %d\n", draw_hdc, read_hdc, ctx, ret );
    return ret;
}

static BOOL egldrv_wglShareLists(struct wgl_context *org, struct wgl_context *dest)
{
    TRACE("(%p, %p)\n", org, dest);

    /* Sharing of display lists works differently in GLX and WGL. In case of GLX it is done
     * at context creation time but in case of WGL it is done using wglShareLists.
     * In the past we tried to emulate wglShareLists by delaying GLX context creation until
     * either a wglMakeCurrent or wglShareLists. This worked fine for most apps but it causes
     * issues for OpenGL 3 because there wglCreateContextAttribsARB can fail in a lot of cases,
     * so there delaying context creation doesn't work.
     *
     * The new approach is to create a GLX context in wglCreateContext / wglCreateContextAttribsARB
     * and when a program requests sharing we recreate the destination context if it hasn't been made
     * current or when it hasn't shared display lists before.
     */

    if ((org->has_been_current && dest->has_been_current) || dest->has_been_current)
    {
        ERR("Could not share display lists, one of the contexts has been current already!\n");
        return FALSE;
    }
    else if (dest->sharing)
    {
        ERR("Could not share display lists because the second context has already shared lists before.\n");
        return FALSE;
    }
    else
    {
        /* Re-create the GLX context and share display lists */
        peglDestroyContext(display, dest->ctx.egl);
        dest->ctx.egl = create_eglcontext(gdi_display, dest, org->ctx.egl);
        TRACE("Re-created context (%p) for Wine context %p (%s) sharing lists with ctx %p (%s)\n",
              dest->ctx.egl, dest, debugstr_eglconfig(dest->fmt->config.c),
              org->ctx.egl, debugstr_eglconfig(org->fmt->config.c));

        org->sharing = TRUE;
        dest->sharing = TRUE;
        return TRUE;
    }
    return FALSE;
}

static struct wgl_context *egl_wglCreateContextAttribsARB(HDC hdc, struct wgl_context *hShareContext,
        const int* attribList)
{
    struct wgl_context *ret;
    struct gl_drawable *gl;
    int err = 0;

    TRACE("(%p %p %p)\n", hdc, hShareContext, attribList);

    if (!(gl = get_gl_drawable(WindowFromDC( hdc ), hdc)))
    {
        SetLastError(ERROR_INVALID_PIXEL_FORMAT);
        return NULL;
    }

    if (!supports_khr_create_context)
        return NULL;

    if ((ret = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*ret))))
    {
        ret->hdc = hdc;
        ret->fmt = gl->format;
        ret->gl3_context = TRUE;
        if (attribList)
        {
            int *egl_attribs = &ret->attribList[0];
            /* attribList consists of pairs {token, value] terminated with 0 */
            while (attribList[0] != 0)
            {
                TRACE("%#x %#x\n", attribList[0], attribList[1]);
                switch(attribList[0])
                {
                    case WGL_CONTEXT_MAJOR_VERSION_ARB:
                        egl_attribs[0] = EGL_CONTEXT_MAJOR_VERSION_KHR;
                        egl_attribs[1] = attribList[1];
                        egl_attribs += 2;
                        ret->numAttribs++;
                        break;
                    case WGL_CONTEXT_MINOR_VERSION_ARB:
                        egl_attribs[0] = EGL_CONTEXT_MINOR_VERSION_KHR;
                        egl_attribs[1] = attribList[1];
                        egl_attribs += 2;
                        ret->numAttribs++;
                        break;
                    case WGL_CONTEXT_LAYER_PLANE_ARB:
                        break;
                    case WGL_CONTEXT_FLAGS_ARB:
                        egl_attribs[0] = EGL_CONTEXT_FLAGS_KHR;
                        egl_attribs[1] = attribList[1];
                        egl_attribs += 2;
                        ret->numAttribs++;
                        break;
                    case WGL_CONTEXT_OPENGL_NO_ERROR_ARB:
                        egl_attribs[0] = EGL_CONTEXT_OPENGL_NO_ERROR_KHR;
                        egl_attribs[1] = attribList[1];
                        egl_attribs += 2;
                        ret->numAttribs++;
                        break;
                    case WGL_CONTEXT_PROFILE_MASK_ARB:
                        if (attribList[1] & WGL_CONTEXT_ES2_PROFILE_BIT_EXT)
                            ret->gles = TRUE;
                        if (attribList[1] & ~WGL_CONTEXT_ES2_PROFILE_BIT_EXT)
                        {
                            egl_attribs[0] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
                            egl_attribs[1] = attribList[1];
                            egl_attribs += 2;
                            ret->numAttribs++;
                        }
                        break;
                    default:
                        ERR("Unhandled attribList pair: %#x %#x\n", attribList[0], attribList[1]);
                }
                attribList += 2;
            }
            egl_attribs[0] = EGL_NONE;
        }

        //X11DRV_expect_error(gdi_display, GLXErrorHandler, NULL);
        ret->ctx.egl = create_eglcontext(display, ret, hShareContext ? hShareContext->ctx.egl : NULL);
        XSync(gdi_display, False);
        if ((err = X11DRV_check_error()) || !ret->ctx.egl)
        {
            /* In the future we should convert the EGL error to a win32 one here if needed */
            WARN("Context creation failed (error %x)\n", err);
            HeapFree(GetProcessHeap(), 0, ret);
            ret = NULL;
        }
        else
        {
            EnterCriticalSection( &context_section );
            list_add_head(&context_list, &ret->entry);
            LeaveCriticalSection( &context_section );
        }
    }

    release_gl_drawable(gl);
    TRACE("%p -> %p\n", hdc, ret);
    return ret;
}

/* FIXME: same as the GLX one, at the moment. */
static const char *egl_wglGetExtensionsStringARB(HDC hdc)
{
    TRACE("() returning \"%s\"\n", wglExtensions);
    return wglExtensions;
}

static struct wgl_pbuffer *egl_wglCreatePbufferARB(HDC hdc, int pixel_format, int width, int height,
        const int *wgl_attribs)
{
    struct wgl_pbuffer* object = NULL;
    const struct wgl_pixel_format *fmt = NULL;
    int attribs[256];
    int nAttribs = 0;

    TRACE("(%p, %d, %d, %d, %p)\n", hdc, pixel_format, width, height, wgl_attribs);

    /* Convert the WGL pixelformat to a GLX format, if it fails then the format is invalid */
    fmt = get_pixel_format(gdi_display, pixel_format, TRUE /* Offscreen */);
    if (!fmt)
    {
        ERR("(%p): invalid pixel format %d\n", hdc, pixel_format);
        SetLastError(ERROR_INVALID_PIXEL_FORMAT);
        return NULL;
    }

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
    {
        SetLastError(ERROR_NO_SYSTEM_RESOURCES);
        return NULL;
    }
    object->width = width;
    object->height = height;
    object->fmt = fmt;

    PUSH2(attribs, EGL_WIDTH, width);
    PUSH2(attribs, EGL_HEIGHT, height);
    while (wgl_attribs && *wgl_attribs)
    {
        int attr_v;

        switch (*wgl_attribs)
        {
            case WGL_PBUFFER_LARGEST_ARB:
                ++wgl_attribs;
                attr_v = *wgl_attribs;
                TRACE("WGL_PBUFFER_LARGEST_ARB = %d\n", attr_v);
                PUSH2(attribs, EGL_LARGEST_PBUFFER, attr_v);
                break;
            case WGL_TEXTURE_FORMAT_ARB:
                ++wgl_attribs;
                attr_v = *wgl_attribs;
                TRACE("WGL_render_texture Attribute: WGL_TEXTURE_FORMAT_ARB as %x\n", attr_v);
                if (attr_v == WGL_NO_TEXTURE_ARB)
                {
                    object->use_render_texture = 0;
                }
                else
                {
                    if (!use_render_texture_emulation)
                    {
                        SetLastError(ERROR_INVALID_DATA);
                        goto create_failed;
                    }
                    switch (attr_v)
                    {
                        case WGL_TEXTURE_RGB_ARB:
                            object->use_render_texture = GL_RGB;
                            object->texture_bpp = 3;
                            object->texture_format = GL_RGB;
                            object->texture_type = GL_UNSIGNED_BYTE;
                            break;
                        case WGL_TEXTURE_RGBA_ARB:
                            object->use_render_texture = GL_RGBA;
                            object->texture_bpp = 4;
                            object->texture_format = GL_RGBA;
                            object->texture_type = GL_UNSIGNED_BYTE;
                            break;
                        /* WGL_FLOAT_COMPONENTS_NV */
                        case WGL_TEXTURE_FLOAT_R_NV:
                            object->use_render_texture = GL_FLOAT_R_NV;
                            object->texture_bpp = 4;
                            object->texture_format = GL_RED;
                            object->texture_type = GL_FLOAT;
                            break;
                        case WGL_TEXTURE_FLOAT_RG_NV:
                            object->use_render_texture = GL_FLOAT_RG_NV;
                            object->texture_bpp = 8;
                            object->texture_format = GL_LUMINANCE_ALPHA;
                            object->texture_type = GL_FLOAT;
                            break;
                        case WGL_TEXTURE_FLOAT_RGB_NV:
                            object->use_render_texture = GL_FLOAT_RGB_NV;
                            object->texture_bpp = 12;
                            object->texture_format = GL_RGB;
                            object->texture_type = GL_FLOAT;
                            break;
                        case WGL_TEXTURE_FLOAT_RGBA_NV:
                            object->use_render_texture = GL_FLOAT_RGBA_NV;
                            object->texture_bpp = 16;
                            object->texture_format = GL_RGBA;
                            object->texture_type = GL_FLOAT;
                            break;
                        default:
                            ERR("Unknown texture format: %x\n", attr_v);
                            SetLastError(ERROR_INVALID_DATA);
                            goto create_failed;
                    }
                    break;
                }
            case WGL_TEXTURE_TARGET_ARB:
                ++wgl_attribs;
                attr_v = *wgl_attribs;
                TRACE("WGL_render_texture Attribute: WGL_TEXTURE_TARGET_ARB as %x\n", attr_v);
                if (attr_v == WGL_NO_TEXTURE_ARB)
                {
                    object->texture_target = 0;
                }
                else
                {
                    if (!use_render_texture_emulation)
                    {
                        SetLastError(ERROR_INVALID_DATA);
                        goto create_failed;
                    }
                    switch (attr_v)
                    {
                        case WGL_TEXTURE_CUBE_MAP_ARB:
                            if (width != height)
                            {
                                SetLastError(ERROR_INVALID_DATA);
                                goto create_failed;
                            }
                            object->texture_target = GL_TEXTURE_CUBE_MAP;
                            object->texture_bind_target = GL_TEXTURE_BINDING_CUBE_MAP;
                           break;
                        case WGL_TEXTURE_1D_ARB:
                            if (height != 1)
                            {
                                SetLastError(ERROR_INVALID_DATA);
                                goto create_failed;
                            }
                            object->texture_target = GL_TEXTURE_1D;
                            object->texture_bind_target = GL_TEXTURE_BINDING_1D;
                            break;
                        case WGL_TEXTURE_2D_ARB:
                            object->texture_target = GL_TEXTURE_2D;
                            object->texture_bind_target = GL_TEXTURE_BINDING_2D;
                            break;
                        case WGL_TEXTURE_RECTANGLE_NV:
                            object->texture_target = GL_TEXTURE_RECTANGLE_NV;
                            object->texture_bind_target = GL_TEXTURE_BINDING_RECTANGLE_NV;
                            break;
                        default:
                            ERR("Unknown texture target: %x\n", attr_v);
                            SetLastError(ERROR_INVALID_DATA);
                            goto create_failed;
                    }
                }
                break;
            case WGL_MIPMAP_TEXTURE_ARB:
                ++wgl_attribs;
                attr_v = *wgl_attribs;
                TRACE("WGL_render_texture Attribute: WGL_MIPMAP_TEXTURE_ARB as %x\n", attr_v);
                if (!use_render_texture_emulation)
                {
                    SetLastError(ERROR_INVALID_DATA);
                    goto create_failed;
                }
                break;
        }
        ++wgl_attribs;
    }

    PUSH1(attribs, EGL_NONE);
    object->pbuffer.surface = peglCreatePbufferSurface(display, fmt->config.c, attribs);
    TRACE("new Pbuffer surface as %p\n", object->pbuffer.surface);
    if (!object->pbuffer.surface)
    {
        SetLastError(ERROR_NO_SYSTEM_RESOURCES);
        goto create_failed; /* unexpected error */
    }
    TRACE("->(%p)\n", object);
    return object;

create_failed:
    HeapFree(GetProcessHeap(), 0, object);
    TRACE("->(FAILED)\n");
    return NULL;
}

static BOOL egl_wglDestroyPbufferARB(struct wgl_pbuffer *object)
{
    EGLBoolean r;

    TRACE("(%p)\n", object);
    r = peglDestroySurface(display, object->pbuffer.surface);
    HeapFree(GetProcessHeap(), 0, object);
    return r;
}

static HDC egl_wglGetPbufferDCARB(struct wgl_pbuffer *object)
{
    FIXME("Not supported.\n");

    return FALSE;
}

static BOOL egl_wglQueryPbufferARB(struct wgl_pbuffer *object, int attrib, int *value)
{
    TRACE("(%p, 0x%x, %p)\n", object, attrib, value);

    switch (attrib)
    {
        case WGL_PBUFFER_WIDTH_ARB:
            peglQuerySurface(display, object->pbuffer.surface, EGL_WIDTH, value);
            break;
        case WGL_PBUFFER_HEIGHT_ARB:
            peglQuerySurface(display, object->pbuffer.surface, EGL_HEIGHT, value);
            break;
        case WGL_PBUFFER_LOST_ARB:
            /* EGL Pbuffers cannot be lost. */
            *value = GL_FALSE;
            break;
        case WGL_TEXTURE_FORMAT_ARB:
            if (!object->use_render_texture)
            {
                *value = WGL_NO_TEXTURE_ARB;
            }
            else
            {
                if (!use_render_texture_emulation)
                {
                    SetLastError(ERROR_INVALID_HANDLE);
                    return GL_FALSE;
                }
                switch (object->use_render_texture)
                {
                    case GL_RGB:
                        *value = WGL_TEXTURE_RGB_ARB;
                        break;
                    case GL_RGBA:
                        *value = WGL_TEXTURE_RGBA_ARB;
                        break;
                    /* WGL_FLOAT_COMPONENTS_NV */
                    case GL_FLOAT_R_NV:
                        *value = WGL_TEXTURE_FLOAT_R_NV;
                        break;
                    case GL_FLOAT_RG_NV:
                        *value = WGL_TEXTURE_FLOAT_RG_NV;
                        break;
                    case GL_FLOAT_RGB_NV:
                        *value = WGL_TEXTURE_FLOAT_RGB_NV;
                        break;
                    case GL_FLOAT_RGBA_NV:
                        *value = WGL_TEXTURE_FLOAT_RGBA_NV;
                        break;
                    default:
                        ERR("Unknown texture format: %x\n", object->use_render_texture);
                }
            }
            break;
        case WGL_TEXTURE_TARGET_ARB:
            if (!object->texture_target)
            {
                *value = WGL_NO_TEXTURE_ARB;
            }
            else
            {
                if (!use_render_texture_emulation)
                {
                    SetLastError(ERROR_INVALID_DATA);
                    return GL_FALSE;
                }
                switch (object->texture_target)
                {
                    case GL_TEXTURE_1D:       *value = WGL_TEXTURE_1D_ARB; break;
                    case GL_TEXTURE_2D:       *value = WGL_TEXTURE_2D_ARB; break;
                    case GL_TEXTURE_CUBE_MAP: *value = WGL_TEXTURE_CUBE_MAP_ARB; break;
                    case GL_TEXTURE_RECTANGLE_NV: *value = WGL_TEXTURE_RECTANGLE_NV; break;
                }
            }
            break;
        case WGL_MIPMAP_TEXTURE_ARB:
            *value = GL_FALSE; /** don't support that */
            FIXME("unsupported WGL_ARB_render_texture attribute query for 0x%x\n", attrib);
            break;
        default:
            FIXME("unexpected attribute %x\n", attrib);
            break;
    }

    return GL_TRUE;
}

static int egl_wglReleasePbufferDCARB(struct wgl_pbuffer *object, HDC hdc)
{
    struct gl_drawable *gl;

    TRACE("(%p, %p)\n", object, hdc);

    EnterCriticalSection(&context_section);

    if (!XFindContext(gdi_display, (XID)hdc, gl_pbuffer_context, (char **)&gl))
    {
        XDeleteContext(gdi_display, (XID)hdc, gl_pbuffer_context);
        free_egl_drawable(gl);
    }
    else hdc = 0;

    LeaveCriticalSection( &context_section );

    return hdc && DeleteDC(hdc);
}

static BOOL egl_wglSetPbufferAttribARB(struct wgl_pbuffer *object, const int *wgl_attribs)
{
    GLboolean ret = GL_FALSE;

    FIXME("(%p, %p) stub\n", object, wgl_attribs);

    if (!object->use_render_texture)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return GL_FALSE;
    }
    if (use_render_texture_emulation)
        return GL_TRUE;
    return ret;
}

static BOOL egl_wglChoosePixelFormatARB(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList,
        UINT nMaxFormats, int *piFormats, UINT *nNumFormats)
{
    int attribs[256];
    int nAttribs = 0;
    EGLConfig *cfgs = NULL;
    int nCfgs = 0;
    int it;
    int fmt_id;
    int start, end;
    UINT pfmt_it = 0;
    int run;
    int i;
    DWORD dwFlags = 0;

    TRACE("(%p, %p, %p, %d, %p, %p): hackish\n", hdc, piAttribIList, pfAttribFList, nMaxFormats, piFormats, nNumFormats);
    if (pfAttribFList)
    {
        FIXME("unused pfAttribFList\n");
    }

    nAttribs = wgl_attribs_to_egl(piAttribIList, attribs, NULL);
    if (nAttribs == -1)
    {
        WARN("Cannot convert WGL to GLX attributes\n");
        return GL_FALSE;
    }
    PUSH1(attribs, EGL_NONE);

    /* There is no 1:1 mapping between GLX and WGL formats because we duplicate some GLX formats for bitmap rendering (see get_formats).
     * Flags like PFD_SUPPORT_GDI, PFD_DRAW_TO_BITMAP and others are a property of the pixel format. We don't query these attributes
     * using glXChooseFBConfig but we filter the result of glXChooseFBConfig later on.
     */
    for (i = 0; piAttribIList[i]; i += 2)
    {
        switch (piAttribIList[i])
        {
            case WGL_DRAW_TO_BITMAP_ARB:
                if(piAttribIList[i+1])
                    dwFlags |= PFD_DRAW_TO_BITMAP;
                break;
            case WGL_ACCELERATION_ARB:
                switch(piAttribIList[i+1])
                {
                    case WGL_NO_ACCELERATION_ARB:
                        dwFlags |= PFD_GENERIC_FORMAT;
                        break;
                    case WGL_GENERIC_ACCELERATION_ARB:
                        dwFlags |= PFD_GENERIC_ACCELERATED;
                        break;
                    case WGL_FULL_ACCELERATION_ARB:
                        /* Nothing to do */
                        break;
                }
                break;
            case WGL_SUPPORT_GDI_ARB:
                if(piAttribIList[i+1])
                    dwFlags |= PFD_SUPPORT_GDI;
                break;
        }
    }

    /* Search for configurations matching the requirements in attribs */
    peglChooseConfig(display, attribs, NULL, 0, &nCfgs);
    if (!nCfgs)
    {
        WARN("Compatible Pixel Format not found\n");
        return GL_FALSE;
    }
    HeapAlloc(GetProcessHeap(), 0, sizeof(*cfgs) * nCfgs);
    peglChooseConfig(display, attribs, cfgs, nCfgs, &nCfgs);

    /* Loop through all matching formats and check if they are suitable.
     * Note that this function should at max return nMaxFormats different formats */
    for (run = 0; run < 2; ++run)
    {
        for (it = 0; it < nCfgs && pfmt_it < nMaxFormats; ++it)
        {
            if (peglGetConfigAttrib(display, cfgs[it], EGL_NATIVE_VISUAL_ID, &fmt_id))
            {
                ERR("Failed to retrieve NATIVE_VISUAL_ID from EGLConfig, expect problems.\n");
                continue;
            }

            /* During the first run we only want onscreen formats and during the second only offscreen */
            start = run == 1 ? nb_onscreen_formats : 0;
            end = run == 1 ? nb_pixel_formats : nb_onscreen_formats;

            for (i = start; i < end; ++i)
            {
                if (pixel_formats[i].fmt_id == fmt_id && (pixel_formats[i].dwFlags & dwFlags) == dwFlags)
                {
                    piFormats[pfmt_it++] = i + 1;
                    TRACE("at %d/%d found NATIVE_VISUAL_ID 0x%x (%d)\n",
                          it + 1, nCfgs, fmt_id, piFormats[pfmt_it]);
                    break;
                }
            }
        }
    }

    *nNumFormats = pfmt_it;
    HeapFree(GetProcessHeap(), 0, cfgs);
    return GL_TRUE;
}

static BOOL egl_wglGetPixelFormatAttribivARB(HDC hdc, int iPixelFormat, int iLayerPlane,
        UINT nAttributes, const int *piAttributes, int *piValues)
{
    UINT i;
    const struct wgl_pixel_format *fmt = NULL;
    EGLBoolean r;
    int tmp;
    int curEGLAttr = 0;

    TRACE("(%p, %d, %d, %d, %p, %p)\n", hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, piValues);

    if (iLayerPlane)
    {
        FIXME("unsupported iLayerPlane(%d) > 0, returning FALSE\n", iLayerPlane);
        return GL_FALSE;
    }

    /* Convert the WGL pixelformat to a EGL one, if this fails then most likely the iPixelFormat isn't supported.
    * We don't have to fail yet as a program can specify an invalid iPixelFormat (lets say 0) if it wants to query
    * the number of supported WGL formats. Whether the iPixelFormat is valid is handled in the for-loop below. */
    fmt = get_pixel_format(gdi_display, iPixelFormat, TRUE /* Offscreen */);
    if (!fmt)
        WARN("Unable to convert iPixelFormat %d to an EGL one!\n", iPixelFormat);

    for (i = 0; i < nAttributes; ++i)
    {
        const int curWGLAttr = piAttributes[i];

        TRACE("pAttr[%d] = %x\n", i, curWGLAttr);
        switch (curWGLAttr)
        {
            case WGL_NUMBER_PIXEL_FORMATS_ARB:
                piValues[i] = nb_pixel_formats;
                continue;
            case WGL_SUPPORT_OPENGL_ARB:
                piValues[i] = GL_TRUE;
                continue;
            case WGL_ACCELERATION_ARB:
                //curEGLAttr = EGL_CONFIG_CAVEAT;
                if (!fmt) goto pix_error;
                if (fmt->dwFlags & PFD_GENERIC_FORMAT)
                    piValues[i] = WGL_NO_ACCELERATION_ARB;
                else if (fmt->dwFlags & PFD_GENERIC_ACCELERATED)
                    piValues[i] = WGL_GENERIC_ACCELERATION_ARB;
                else
                    piValues[i] = WGL_FULL_ACCELERATION_ARB;
                continue;
            case WGL_TRANSPARENT_ARB:
                curEGLAttr = EGL_TRANSPARENT_TYPE;
                if (!fmt) goto pix_error;
                r = peglGetConfigAttrib(display, fmt->config.c, curEGLAttr, &tmp);
                if (!r) goto get_error;
                piValues[i] = GL_FALSE;
                if (tmp != EGL_NONE) piValues[i] = GL_TRUE;
                continue;
            case WGL_PIXEL_TYPE_ARB:
                TRACE("WGL_PIXEL_TYPE_ARB: GLX_RENDER_TYPE = WGL_TYPE_RGBA_ARB\n");
                piValues[i] = WGL_TYPE_RGBA_ARB;
                continue;
            case WGL_COLOR_BITS_ARB:
                curEGLAttr = EGL_BUFFER_SIZE;
                break;
            case WGL_BIND_TO_TEXTURE_RGB_ARB:
            case WGL_BIND_TO_TEXTURE_RGBA_ARB:
                if (!use_render_texture_emulation)
                {
                    piValues[i] = GL_FALSE;
                    continue;
                }
                r = peglGetConfigAttrib(display, fmt->config.c, EGL_SURFACE_TYPE, &tmp);
                if (!r) goto get_error;
                piValues[i] = (tmp & EGL_PBUFFER_BIT) ? GL_TRUE : GL_FALSE;
                continue;
            case WGL_BLUE_BITS_ARB:
                curEGLAttr = EGL_BLUE_SIZE;
                break;
            case WGL_RED_BITS_ARB:
                curEGLAttr = EGL_RED_SIZE;
                break;
            case WGL_GREEN_BITS_ARB:
                curEGLAttr = EGL_GREEN_SIZE;
                break;
            case WGL_ALPHA_BITS_ARB:
                curEGLAttr = EGL_ALPHA_SIZE;
                break;
            case WGL_DEPTH_BITS_ARB:
                curEGLAttr = EGL_DEPTH_SIZE;
                break;
            case WGL_STENCIL_BITS_ARB:
                curEGLAttr = EGL_STENCIL_SIZE;
                break;
            case WGL_DOUBLE_BUFFER_ARB:
                piValues[i] = GL_TRUE;
                continue;
            case WGL_STEREO_ARB:
                piValues[i] = GL_FALSE;
                continue;
            case WGL_AUX_BUFFERS_ARB:
                piValues[i] = 0;
                continue;
            case WGL_SUPPORT_GDI_ARB:
                if (!fmt) goto pix_error;
                piValues[i] = (fmt->dwFlags & PFD_SUPPORT_GDI) != 0;
                continue;
            case WGL_DRAW_TO_BITMAP_ARB:
                if (!fmt) goto pix_error;
                piValues[i] = (fmt->dwFlags & PFD_DRAW_TO_BITMAP) != 0;
                continue;
            case WGL_DRAW_TO_WINDOW_ARB:
            case WGL_DRAW_TO_PBUFFER_ARB:
                if (!fmt) goto pix_error;
                r = peglGetConfigAttrib(display, fmt->config.c, EGL_SURFACE_TYPE, &tmp);
                if (!r) goto get_error;
                if((curWGLAttr == WGL_DRAW_TO_WINDOW_ARB && (tmp & EGL_WINDOW_BIT))
                        || (curWGLAttr == WGL_DRAW_TO_PBUFFER_ARB && (tmp & EGL_PBUFFER_BIT)))
                    piValues[i] = GL_TRUE;
                else
                    piValues[i] = GL_FALSE;
                continue;
            case WGL_SWAP_METHOD_ARB:
                /* FIXME: For now return SWAP_EXCHANGE_ARB. */
                piValues[i] = WGL_SWAP_EXCHANGE_ARB;
                continue;
            case WGL_PBUFFER_LARGEST_ARB:
                /* FIXME: ? */
                curEGLAttr = EGL_LARGEST_PBUFFER;
                break;
            case WGL_SAMPLE_BUFFERS_ARB:
                curEGLAttr = EGL_SAMPLE_BUFFERS;
                break;
            case WGL_SAMPLES_ARB:
                curEGLAttr = EGL_SAMPLES;
                break;
            case WGL_FLOAT_COMPONENTS_NV:
                piValues[i] = 0;
                continue;
            case WGL_FRAMEBUFFER_SRGB_CAPABLE_EXT:
                piValues[i] = GL_TRUE;
                continue;
            case WGL_TYPE_RGBA_UNSIGNED_FLOAT_EXT:
                piValues[i] = 0;
                continue;
            case WGL_ACCUM_RED_BITS_ARB:
                piValues[i] = 0;
                continue;
            case WGL_ACCUM_GREEN_BITS_ARB:
                piValues[i] = 0;
                continue;
            case WGL_ACCUM_BLUE_BITS_ARB:
                piValues[i] = 0;
                continue;
            case WGL_ACCUM_ALPHA_BITS_ARB:
                piValues[i] = 0;
                continue;
            case WGL_ACCUM_BITS_ARB:
                piValues[i] = 0;
                continue;
            default:
                FIXME("unsupported %x WGL Attribute\n", curWGLAttr);
        }

        /* Retrieve a GLX FBConfigAttrib when the attribute to query is valid and
         * iPixelFormat != 0. When iPixelFormat is 0 the only value which makes
         * sense to query is WGL_NUMBER_PIXEL_FORMATS_ARB.
         *
         * TODO: properly test the behavior of wglGetPixelFormatAttrib*v on Windows
         *       and check which options can work using iPixelFormat=0 and which not.
         *       A problem would be that this function is an extension. This would
         *       mean that the behavior could differ between different vendors (ATI, Nvidia, ..).
         */
        if (curEGLAttr && iPixelFormat)
        {
            if (!fmt) goto pix_error;
            r = peglGetConfigAttrib(display, fmt->config.c, curEGLAttr, piValues + i);
            if (!r) goto get_error;
            curEGLAttr = 0;
        }
        else
        {
            piValues[i] = GL_FALSE;
        }
    }
    return GL_TRUE;

get_error:
    ERR("(%p): unexpected failure on eglGetConfigAttrib(%#x), returning FALSE\n", hdc, curEGLAttr);
    return GL_FALSE;

pix_error:
    ERR("(%p): unexpected iPixelFormat(%d) vs nFormats(%d), returning FALSE\n", hdc, iPixelFormat, nb_pixel_formats);
    return GL_FALSE;
}

static BOOL egl_wglGetPixelFormatAttribfvARB(HDC hdc, int iPixelFormat, int iLayerPlane,
        UINT nAttributes, const int *piAttributes, FLOAT *pfValues)
{
    int *attr;
    int ret;
    UINT i;

    TRACE("(%p, %d, %d, %d, %p, %p)\n", hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, pfValues);

    /* Allocate a temporary array to store integer values */
    attr = HeapAlloc(GetProcessHeap(), 0, nAttributes * sizeof(int));
    if (!attr) {
        ERR("couldn't allocate %d array\n", nAttributes);
        return GL_FALSE;
    }

    /* Piggy-back on wglGetPixelFormatAttribivARB */
    ret = egl_wglGetPixelFormatAttribivARB(hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, attr);
    if (ret)
    {
        /* Convert integer values to float. Should also check for attributes
           that can give decimal values here */
        for (i = 0; i < nAttributes; ++i)
        {
            pfValues[i] = attr[i];
        }
    }

    HeapFree(GetProcessHeap(), 0, attr);
    return ret;
}

/* static BOOL egl_wglBindTexImageARB(struct wgl_pbuffer *object, int iBuffer) */
/* { */
/*     WARN("(%p, %d) unsupported\n", object, iBuffer); */
/*     return GL_FALSE; */
/* } */

/* static BOOL egl_wglReleaseTexImageARB( struct wgl_pbuffer *object, int iBuffer ) */
/* { */
/*     WARN("(%p, %d) unsupported\n", object, iBuffer); */
/*     return GL_FALSE; */
/* } */

static const char *egl_wglGetExtensionsStringEXT(void)
{
    TRACE("() returning \"%s\"\n", wglExtensions);
    return wglExtensions;
}

static int egl_wglGetSwapIntervalEXT(void)
{
    struct wgl_context *ctx = NtCurrentTeb()->glContext;
    struct gl_drawable *gl;
    int swap_interval;

    TRACE("()\n");

    if (!(gl = get_gl_drawable(WindowFromDC( ctx->hdc ), ctx->hdc)))
    {
        /* This can't happen because a current WGL context is required to get
         * here. Likely the application is buggy.
         */
        WARN("No GL drawable found, returning swap interval 0.\n");
        return 0;
    }

    swap_interval = gl->swap_interval;
    release_gl_drawable(gl);

    return swap_interval;
}

static BOOL egl_wglSwapIntervalEXT(int interval)
{
    struct wgl_context *ctx = NtCurrentTeb()->glContext;
    struct gl_drawable *gl;
    BOOL ret = TRUE;

    TRACE("(%d)\n", interval);

    if (interval < 0)
    {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }

    if (!(gl = get_gl_drawable(WindowFromDC(ctx->hdc), ctx->hdc)))
    {
        SetLastError(ERROR_DC_NOT_FOUND);
        return FALSE;
    }

    ret = peglSwapInterval(display, interval);

    if (ret)
        gl->swap_interval = interval;
    else
        SetLastError(ERROR_DC_NOT_FOUND);

    release_gl_drawable(gl);

    return ret;
}

/**
 * egl_wglSetPixelFormatWINE
 *
 * WGL_WINE_pixel_format_passthrough: wglSetPixelFormatWINE
 * This is a WINE-specific wglSetPixelFormat which can set the pixel format multiple times.
 */
static BOOL egl_wglSetPixelFormatWINE(HDC hdc, int format)
{
    const struct wgl_pixel_format *fmt;
    int value;
    HWND hwnd;

    TRACE("(%p,%d)\n", hdc, format);

    fmt = get_pixel_format(gdi_display, format, FALSE /* Offscreen */);
    if (!fmt)
    {
        ERR("Invalid format %d\n", format);
        return FALSE;
    }

    hwnd = WindowFromDC( hdc );
    if (!hwnd || hwnd == GetDesktopWindow())
    {
        ERR( "not a valid window DC %p\n", hdc );
        return FALSE;
    }

    peglGetConfigAttrib(display, fmt->config.c, EGL_SURFACE_TYPE, &value);
    if (!(value & EGL_WINDOW_BIT))
    {
        WARN( "Pixel format %d is not compatible for window rendering\n", format );
        return FALSE;
    }

    return egl_set_win_format(hwnd, fmt);
}

static void egl_WineGL_LoadExtensions(void)
{
    wglExtensions[0] = 0;

    /* ARB Extensions */
    egl_funcs.ext.p_wglCreateContextAttribsARB = egl_wglCreateContextAttribsARB;
    if (has_extension(glxExtensions, "EGL_KHR_create_context"))
    {
        register_extension("WGL_ARB_create_context");
        register_extension("WGL_ARB_create_context_profile");
        supports_khr_create_context = TRUE;
    }

    register_extension("WGL_ARB_extensions_string");
    egl_funcs.ext.p_wglGetExtensionsStringARB = egl_wglGetExtensionsStringARB;

    register_extension("WGL_ARB_make_current_read");
    egl_funcs.ext.p_wglGetCurrentReadDCARB   = (void *)1;  /* never called */
    egl_funcs.ext.p_wglMakeContextCurrentARB = egl_wglMakeContextCurrentARB;

    register_extension("WGL_ARB_pbuffer");
    egl_funcs.ext.p_wglCreatePbufferARB    = egl_wglCreatePbufferARB;
    egl_funcs.ext.p_wglDestroyPbufferARB   = egl_wglDestroyPbufferARB;
    egl_funcs.ext.p_wglGetPbufferDCARB     = egl_wglGetPbufferDCARB;
    egl_funcs.ext.p_wglQueryPbufferARB     = egl_wglQueryPbufferARB;
    egl_funcs.ext.p_wglReleasePbufferDCARB = egl_wglReleasePbufferDCARB;
    egl_funcs.ext.p_wglSetPbufferAttribARB = egl_wglSetPbufferAttribARB;

    register_extension("WGL_ARB_pixel_format");
    egl_funcs.ext.p_wglChoosePixelFormatARB      = egl_wglChoosePixelFormatARB;
    egl_funcs.ext.p_wglGetPixelFormatAttribfvARB = egl_wglGetPixelFormatAttribfvARB;
    egl_funcs.ext.p_wglGetPixelFormatAttribivARB = egl_wglGetPixelFormatAttribivARB;

    /* Support WGL_ARB_render_texture when there's support or pbuffer based emulation */
    /* if (has_extension( WineGLInfo.glxExtensions, "GLX_ARB_render_texture") || */
    /*     (glxRequireVersion(3) && has_extension( WineGLInfo.glxExtensions, "GLX_SGIX_pbuffer") && use_render_texture_emulation)) */
    /* { */
    /*     register_extension( "WGL_ARB_render_texture" ); */
    /*     egl_funcs.ext.p_wglBindTexImageARB    = X11DRV_wglBindTexImageARB; */
    /*     egl_funcs.ext.p_wglReleaseTexImageARB = X11DRV_wglReleaseTexImageARB; */

    /*     /\* The WGL version of GLX_NV_float_buffer requires render_texture *\/ */
    /*     if (has_extension( WineGLInfo.glxExtensions, "GLX_NV_float_buffer")) */
    /*         register_extension("WGL_NV_float_buffer"); */

    /*     /\* Again there's no GLX equivalent for this extension, so depend on the required GL extension *\/ */
    /*     if (has_extension(WineGLInfo.glExtensions, "GL_NV_texture_rectangle")) */
    /*         register_extension("WGL_NV_render_texture_rectangle"); */
    /* } */

    /* EXT Extensions */

    register_extension("WGL_EXT_extensions_string");
    egl_funcs.ext.p_wglGetExtensionsStringEXT = egl_wglGetExtensionsStringEXT;

    /* Load this extension even when it isn't backed by a GLX extension because it is has been around for ages.
     * Games like Call of Duty and K.O.T.O.R. rely on it. Further our emulation is good enough. */
    register_extension("WGL_EXT_swap_control");
    egl_funcs.ext.p_wglSwapIntervalEXT = egl_wglSwapIntervalEXT;
    egl_funcs.ext.p_wglGetSwapIntervalEXT = egl_wglGetSwapIntervalEXT;

    register_extension("WGL_EXT_framebuffer_sRGB");

    /* WINE-specific WGL Extensions */

    /* In WineD3D we need the ability to set the pixel format more than once (e.g. after a device reset).
     * The default wglSetPixelFormat doesn't allow this, so add our own which allows it.
     */
    register_extension("WGL_WINE_pixel_format_passthrough");
    egl_funcs.ext.p_wglSetPixelFormatWINE = egl_wglSetPixelFormatWINE;
}

static BOOL egldrv_wglSwapBuffers(HDC hdc)
{
    struct x11drv_escape_flush_gl_drawable escape;
    struct gl_drawable *gl;
    struct wgl_context *ctx = NtCurrentTeb()->glContext;

    TRACE("(%p)\n", hdc);

    escape.code = X11DRV_FLUSH_GL_DRAWABLE;
    escape.gl_drawable = 0;

    if (!(gl = get_gl_drawable(WindowFromDC(hdc), hdc)))
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    switch (gl->type)
    {
        case DC_GL_PIXMAP_WIN:
            if (ctx) egl_sync_context(ctx);
            escape.gl_drawable = gl->pixmap;
            peglSwapBuffers(display, gl->drawable.s);
            break;
        case DC_GL_CHILD_WIN:
            escape.gl_drawable = gl->drawable.d;
            /* fall through */
        default:
            peglSwapBuffers(display, gl->drawable.s);
            break;
    }

    release_gl_drawable(gl);

    if (escape.gl_drawable) ExtEscape(ctx->hdc, X11DRV_ESCAPE, sizeof(escape), (LPSTR)&escape, 0, NULL);
    return TRUE;
}


static struct opengl_funcs egl_funcs =
{
    {
        egldrv_wglCopyContext,
        egldrv_wglCreateContext,
        egldrv_wglDeleteContext,
        egldrv_wglDescribePixelFormat,
        egldrv_wglGetPixelFormat,
        egldrv_wglGetProcAddress,
        egldrv_wglMakeCurrent,
        egldrv_wglSetPixelFormat,
        egldrv_wglShareLists,
        egldrv_wglSwapBuffers,
    }
};

struct opengl_funcs *get_wgl_driver( UINT version )
{
    if (version != WINE_WGL_DRIVER_VERSION)
    {
        ERR( "version mismatch, opengl32 wants %u but driver has %u\n", version, WINE_WGL_DRIVER_VERSION );
        return NULL;
    }
    if (has_egl()) return &egl_funcs;
    else if (has_opengl()) return &opengl_funcs;
    return NULL;
}

#else /* defined(SONAME_LIBEGL) */

struct opengl_funcs *get_wgl_driver( UINT version )
{
    if (version != WINE_WGL_DRIVER_VERSION)
    {
        ERR( "version mismatch, opengl32 wants %u but driver has %u\n", version, WINE_WGL_DRIVER_VERSION );
        return NULL;
    }
    if (has_opengl()) return &opengl_funcs;
    return NULL;
}

#endif /* defined(SONAME_LIBEGL) */

#else  /* no OpenGL includes */

struct opengl_funcs *get_wgl_driver( UINT version )
{
    return NULL;
}

void sync_gl_drawable( HWND hwnd )
{
}

void set_gl_drawable_parent( HWND hwnd, HWND parent )
{
}

void destroy_gl_drawable( HWND hwnd )
{
}

#endif /* defined(SONAME_LIBGL) */
