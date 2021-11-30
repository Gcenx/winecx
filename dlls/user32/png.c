/*
 * PNG icon support
 *
 * Copyright 2018 Dmitry Timoshkov
 * Copyright 2019 Alexandre Julliard
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

#if 0
#pragma makedep unix
#endif

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#ifdef SONAME_LIBPNG
#include <png.h>
#endif
#include <errno.h>
#include <poll.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winerror.h"
#include "winnls.h"
#include "winternl.h"
#include "wine/exception.h"
#include "wine/server.h"
#include "controls.h"
#include "win.h"
#include "user_private.h"
#include "wine/list.h"
#include "wine/unicode.h"
#include "wine/debug.h"

#ifdef SONAME_LIBPNG

WINE_DEFAULT_DEBUG_CHANNEL(cursor);

/* CX Hack 9660:
 * Search for more soname names than the one
 * that we happened to build Wine against. */
static struct {
    const char *soname;
    const char *verstring;
} libpng_candidates[] = {
    { SONAME_LIBPNG, PNG_LIBPNG_VER_STRING },
    { "libpng16.so", "1.6.0" },
    { "libpng16.so.0", "1.6.0" },
    { "libpng16.so.16", "1.6.0" },
    { "libpng15.so", "1.5.0" },
    { "libpng15.so.0", "1.5.0" },
    { "libpng15.so.15", "1.5.0" },
    { "libpng14.so", "1.4.0" },
    { "libpng14.so.0", "1.4.0" },
    { "libpng14.so.14", "1.4.0" },
    { "libpng12.so", "1.2.0" },
    { "libpng12.so.0", "1.2.0" },
    { "libpng12.so.12", "1.2.0" },
};

static const char *soname_libpng;
static const char *libpng_ver_string;

static void * HOSTPTR libpng_handle;
#define MAKE_FUNCPTR(f) static typeof(f) * HOSTPTR p##f
MAKE_FUNCPTR(png_create_read_struct);
MAKE_FUNCPTR(png_create_info_struct);
MAKE_FUNCPTR(png_destroy_read_struct);
MAKE_FUNCPTR(png_error);
MAKE_FUNCPTR(png_get_bit_depth);
MAKE_FUNCPTR(png_get_color_type);
MAKE_FUNCPTR(png_get_error_ptr);
MAKE_FUNCPTR(png_get_image_height);
MAKE_FUNCPTR(png_get_image_width);
MAKE_FUNCPTR(png_get_io_ptr);
MAKE_FUNCPTR(png_read_image);
MAKE_FUNCPTR(png_read_info);
MAKE_FUNCPTR(png_read_update_info);
MAKE_FUNCPTR(png_set_bgr);
MAKE_FUNCPTR(png_set_crc_action);
MAKE_FUNCPTR(png_set_error_fn);
MAKE_FUNCPTR(png_set_expand);
MAKE_FUNCPTR(png_set_gray_to_rgb);
MAKE_FUNCPTR(png_set_read_fn);
#undef MAKE_FUNCPTR

static void user_error_fn(png_structp png_ptr, png_const_charp error_message)
{
    jmp_buf *pjmpbuf;

    /* This uses setjmp/longjmp just like the default. We can't use the
     * default because there's no way to access the jmp buffer in the png_struct
     * that works in 1.2 and 1.4 and allows us to dynamically load libpng. */
    WARN("PNG error: %s\n", debugstr_a(error_message));
    pjmpbuf = ppng_get_error_ptr(png_ptr);
    longjmp(*pjmpbuf, 1);
}

static void user_warning_fn(png_structp png_ptr, png_const_charp warning_message)
{
    WARN("PNG warning: %s\n", debugstr_a(warning_message));
}

struct png_wrapper
{
    const char *buffer;
    size_t size, pos;
};

static void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
    struct png_wrapper * HOSTPTR png = ppng_get_io_ptr(png_ptr);

    if (png->size - png->pos >= length)
    {
        memcpy(data, png->buffer + png->pos, length);
        png->pos += length;
    }
    else
    {
        ppng_error(png_ptr, "failed to read PNG data");
    }
}

static unsigned be_uint(unsigned val)
{
    union
    {
        unsigned val;
        unsigned char c[4];
    } u;

    u.val = val;
    return (u.c[0] << 24) | (u.c[1] << 16) | (u.c[2] << 8) | u.c[3];
}

static BOOL CDECL get_png_info(const void *png_data, DWORD size, int *width, int *height, int *bpp)
{
    static const char png_sig[8] = { 0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a };
    static const char png_IHDR[8] = { 0,0,0,0x0d,'I','H','D','R' };
    const struct
    {
        char png_sig[8];
        char ihdr_sig[8];
        unsigned width, height;
        char bit_depth, color_type, compression, filter, interlace;
    } *png = png_data;

    if (size < sizeof(*png)) return FALSE;
    if (memcmp(png->png_sig, png_sig, sizeof(png_sig)) != 0) return FALSE;
    if (memcmp(png->ihdr_sig, png_IHDR, sizeof(png_IHDR)) != 0) return FALSE;

    *bpp = (png->color_type == PNG_COLOR_TYPE_RGB_ALPHA) ? 32 : 24;
    *width = be_uint(png->width);
    *height = be_uint(png->height);

    return TRUE;
}

static BITMAPINFO * CDECL load_png(const char *png_data, DWORD *size)
{
    struct png_wrapper png;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep * row_pointers = NULL;
    jmp_buf jmpbuf;
    int color_type, bit_depth, bpp, width, height;
    int rowbytes, image_size, mask_size = 0, i;
    BITMAPINFO *info = NULL;
    unsigned char *image_data;

    if (!get_png_info(png_data, *size, &width, &height, &bpp)) return NULL;

    png.buffer = png_data;
    png.size = *size;
    png.pos = 0;

    /* initialize libpng */
    png_ptr = ppng_create_read_struct(libpng_ver_string, NULL, NULL, NULL);
    if (!png_ptr) return NULL;

    info_ptr = ppng_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        ppng_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    /* set up setjmp/longjmp error handling */
    if (setjmp(jmpbuf))
    {
        free(row_pointers);
        RtlFreeHeap(GetProcessHeap(), 0, info);
        ppng_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    ppng_set_error_fn(png_ptr, jmpbuf, user_error_fn, user_warning_fn);
    ppng_set_crc_action(png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);

    /* set up custom i/o handling */
    ppng_set_read_fn(png_ptr, &png, user_read_data);

    /* read the header */
    ppng_read_info(png_ptr, info_ptr);

    color_type = ppng_get_color_type(png_ptr, info_ptr);
    bit_depth = ppng_get_bit_depth(png_ptr, info_ptr);

    /* expand grayscale image data to rgb */
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        ppng_set_gray_to_rgb(png_ptr);

    /* expand palette image data to rgb */
    if (color_type == PNG_COLOR_TYPE_PALETTE || bit_depth < 8)
        ppng_set_expand(png_ptr);

    /* update color type information */
    ppng_read_update_info(png_ptr, info_ptr);

    color_type = ppng_get_color_type(png_ptr, info_ptr);
    bit_depth = ppng_get_bit_depth(png_ptr, info_ptr);

    bpp = 0;

    switch (color_type)
    {
    case PNG_COLOR_TYPE_RGB:
        if (bit_depth == 8)
            bpp = 24;
        break;

    case PNG_COLOR_TYPE_RGB_ALPHA:
        if (bit_depth == 8)
        {
            ppng_set_bgr(png_ptr);
            bpp = 32;
        }
        break;

    default:
        break;
    }

    if (!bpp)
    {
        FIXME("unsupported PNG color format %d, %d bpp\n", color_type, bit_depth);
        ppng_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    width = ppng_get_image_width(png_ptr, info_ptr);
    height = ppng_get_image_height(png_ptr, info_ptr);

    rowbytes = (width * bpp + 7) / 8;
    image_size = height * rowbytes;
    if (bpp != 32) /* add a mask if there is no alpha */
        mask_size = (width + 7) / 8 * height;

    info = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(BITMAPINFOHEADER) + image_size + mask_size);
    if (!info)
    {
        ppng_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    image_data = (unsigned char *)info + sizeof(BITMAPINFOHEADER);
    memset(image_data + image_size, 0, mask_size);

    row_pointers = malloc(height * sizeof(png_bytep));
    if (!row_pointers)
    {
        RtlFreeHeap(GetProcessHeap(), 0, info);
        ppng_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    /* upside down */
    for (i = 0; i < height; i++)
        row_pointers[i] = image_data + (height - i - 1) * rowbytes;

    ppng_read_image(png_ptr, row_pointers);
    free(row_pointers);
    ppng_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = width;
    info->bmiHeader.biHeight = height * 2;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = bpp;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biSizeImage = image_size;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biClrImportant = 0;

    *size = sizeof(BITMAPINFOHEADER) + image_size + mask_size;
    return info;
}

static const struct png_funcs funcs =
{
    get_png_info,
    load_png
};

/***********************************************************************
 *           MENU_get_menu_socket
 *
 *  CrossOver HACKs for bug 6727.
 *
 *  On OSX, crossover.app can put a socket handle in
 *   the CX_MENU_SOCKET environemtn variable.  If that
 *   value is set then we will make an XML structure out
 *   of the menu bar and pass it upstream so that the mac
 *   application can display a proper mac-style menubar.
 *
 */
static int MENU_get_menu_socket(void)
{
    char * HOSTPTR socketname;

    if ((socketname = getenv("CX_MENU_SOCKET")))
    {
        struct sockaddr_un sa;
        int sock = socket(AF_UNIX,SOCK_STREAM,0);
        TRACE("Found socket %s.\n",socketname);

        sa.sun_family=AF_UNIX;
        if (strlen(socketname) > (sizeof(sa.sun_path)-1))
        {
            TRACE("Socket name %s is too long for us to use!\n", socketname);
            return -1;
        }
        strcpy(sa.sun_path,socketname);

        if (!connect(sock, (struct sockaddr *) &sa, sizeof(sa)))
        {
            /* Make the socket nonblocking.  That prevents us from locking up
               if the Mac App stops listening. */
            int flags = fcntl(sock, F_GETFL);
            if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
            {
                TRACE("Failed to set socket to O_NONBLOCK.\n");
            }

            return sock;
        }
        else
        {
            WINE_WARN("Failed to connect to menu socket %s.  errno: %d\n",socketname,errno);
            return -1;
        }
    }

    return -1;
}

static BOOL MENU_write_data_to_pipe(int sock, const char *data, int len)
{
    int written = 0;
    short revents = 0;

    while (written < len && !(revents & (POLLERR | POLLHUP)))
    {
        struct pollfd pollstruct;
        int ready;

        pollstruct.fd = sock;
        pollstruct.events = POLLOUT;
        pollstruct.revents = revents;

        ready = poll(&pollstruct, 1, 1000);
        if (ready && (ready != -1))
        {
            int thisChunkSize;
            thisChunkSize = write(sock,data+written,len-written);
            if (thisChunkSize == -1)
            {
                TRACE("Failed to write menu info.  errno: %d\n", errno);
                break;
            }
            written += thisChunkSize;
        }
        else
        {
            /*  Timed out.  If the Mac app isn't listening, we
                can just error out... there will be plenty more
                menus where this one came from.  */
            return FALSE;
        }
    }

    if (written < len)
    {
        WARN("Failed to write to menu socket.  errno: %d\n",errno);
        return FALSE;
    }

    return TRUE;
}

/***********************************************************************
 *           send_cx_menu_data
 *
 *  CrossOver Hack for bug 6727.
 */
static void CDECL send_cx_menu_data( const char *data, int len )
{
    int sock = MENU_get_menu_socket();

    if (sock == -1) return;
    MENU_write_data_to_pipe( sock, data, len );
    close( sock );
}

NTSTATUS CDECL __wine_init_unix_lib( HMODULE module, DWORD reason, const void *ptr_in, void *ptr_out )
{
    int i;

    if (reason != DLL_PROCESS_ATTACH) return STATUS_SUCCESS;

    /* CrossOver Hack 6727 */
    if (ptr_in)
    {
        if (!getenv( "CX_MENU_SOCKET" )) return STATUS_DLL_NOT_FOUND;
        *(void **)ptr_out = send_cx_menu_data;
        return STATUS_SUCCESS;
    }

    /* CrossOver Hack 9660 */
    for(i = 0; i < sizeof(libpng_candidates) / sizeof(*libpng_candidates); ++i)
    {
        soname_libpng = libpng_candidates[i].soname;
        libpng_ver_string = libpng_candidates[i].verstring;
        libpng_handle = dlopen(soname_libpng, RTLD_NOW);
        if(libpng_handle)
            break;
    }

    if (!(libpng_handle = dlopen( SONAME_LIBPNG, RTLD_NOW )))
    {
        WARN( "failed to load %s\n", SONAME_LIBPNG );
        return STATUS_DLL_NOT_FOUND;
    }
#define LOAD_FUNCPTR(f) \
    if (!(p##f = dlsym( libpng_handle, #f ))) \
    { \
        WARN( "%s not found in %s\n", #f, SONAME_LIBPNG ); \
        return STATUS_PROCEDURE_NOT_FOUND; \
    }
    LOAD_FUNCPTR(png_create_read_struct);
    LOAD_FUNCPTR(png_create_info_struct);
    LOAD_FUNCPTR(png_destroy_read_struct);
    LOAD_FUNCPTR(png_error);
    LOAD_FUNCPTR(png_get_bit_depth);
    LOAD_FUNCPTR(png_get_color_type);
    LOAD_FUNCPTR(png_get_error_ptr);
    LOAD_FUNCPTR(png_get_image_height);
    LOAD_FUNCPTR(png_get_image_width);
    LOAD_FUNCPTR(png_get_io_ptr);
    LOAD_FUNCPTR(png_read_image);
    LOAD_FUNCPTR(png_read_info);
    LOAD_FUNCPTR(png_read_update_info);
    LOAD_FUNCPTR(png_set_bgr);
    LOAD_FUNCPTR(png_set_crc_action);
    LOAD_FUNCPTR(png_set_error_fn);
    LOAD_FUNCPTR(png_set_expand);
    LOAD_FUNCPTR(png_set_gray_to_rgb);
    LOAD_FUNCPTR(png_set_read_fn);
#undef LOAD_FUNCPTR

    *(const struct png_funcs **)ptr_out = &funcs;
    return STATUS_SUCCESS;
}

#endif  /* SONAME_LIBPNG */
