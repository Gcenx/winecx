/*
 * NT process handling
 *
 * Copyright 1996-1998 Marcus Meissner
 * Copyright 2018, 2020 Alexandre Julliard
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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_TIMES_H
# include <sys/times.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#include <stdint.h>
#ifdef HAVE_SYS_UN_H
# include <sys/un.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef __APPLE__
# include <CoreFoundation/CoreFoundation.h>
# include <pthread.h>
#endif
#ifdef HAVE_MACH_MACH_H
# include <mach/mach.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "unix_private.h"
#include "wine/exception.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(process);


static ULONG execute_flags = MEM_EXECUTE_OPTION_DISABLE | (
#ifdef __i386_on_x86_64__
                                                           0 ?
#else
                                                           sizeof(void *) > sizeof(int) ?
#endif
                                                           MEM_EXECUTE_OPTION_DISABLE_THUNK_EMULATION |
                                                           MEM_EXECUTE_OPTION_PERMANENT : 0);

static const char * const cpu_names[] = { "x86", "x86_64", "PowerPC", "ARM", "ARM64" };

static UINT process_error_mode;

/* CrossOver Hack 10523: shunt the loading to CrossOver */
enum { /* must match definitions in Mac app code (WineLoader.m) */
    REQUEST_LOAD_WINE = 0x52c17355,
    RESPONSE_SUCCESS,
    REQUEST_LOAD_WINE_64BIT,
};

static inline BOOL is_64bit_arch( client_cpu_t cpu )
{
    return (cpu == CPU_x86_64 || cpu == CPU_ARM64);
}

static BOOL write_data(int sock, const void * HOSTPTR buffer, size_t length)
{
    const char* HOSTPTR p = buffer;
    while (length)
    {
        ssize_t rc = write(sock, p, length);
        if (rc > 0)
        {
            p += rc;
            length -= rc;
        }
        else if (errno != EINTR)
        {
            WARN("failed to write data; errno %d\n", errno);
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL write_length_prefixed_buffer(int sock, const void * HOSTPTR buffer, uint64_t length)
{
    if (!write_data(sock, &length, sizeof(length)))
        return FALSE;

    if (!write_data(sock, buffer, length))
        return FALSE;

    return TRUE;
}

static BOOL read_data(int sock, void * HOSTPTR buffer, size_t length)
{
    char* HOSTPTR p = buffer;
    while (length > 0)
    {
        int rc = read(sock, p, length);
        if (rc > 0)
        {
            p += rc;
            length -= rc;
        }
        else if (rc == 0 || errno != EINTR)
            return FALSE;
    }

    return TRUE;
}

static char * HOSTPTR * HOSTPTR build_argv( const UNICODE_STRING *cmdline, int reserved );

static BOOL has_key_value( const WCHAR *exenameW, const WCHAR *keyname, BOOL *exists )
{
    KEY_VALUE_BASIC_INFORMATION info;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name;
    HANDLE hkey;
    DWORD size;
    NTSTATUS status;

    name.Length = wcslen(keyname) * sizeof(WCHAR);
    name.Buffer = (WCHAR *)keyname;
    InitializeObjectAttributes( &attr, &name, OBJ_CASE_INSENSITIVE, 0, 0 );
    if (NtOpenKey(&hkey, KEY_READ, &attr)) return FALSE;
    name.Length = wcslen(exenameW) * sizeof(WCHAR);
    name.Buffer = (WCHAR *)exenameW;
    status = NtQueryValueKey(hkey, &name, KeyValueBasicInformation, &info, sizeof(info), &size);
    *exists = (status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW);
    NtClose(hkey);
    return TRUE;
}

static BOOL send_to_cx_loader(const char * HOSTPTR loader, const RTL_USER_PROCESS_PARAMETERS *params,
                              int wineserversocket, int stdin_fd, int stdout_fd, char * HOSTPTR winedebug,
                              const pe_image_info_t *pe_info, const char* HOSTPTR wineloader)
{
    /* HKCU\Software\CrossOver\SuppressAltLoader */
    /* FIXME: root hardcoded for now */
#define HKCU_ROOT '\\','R','e','g','i','s','t','r','y','\\','U','s','e','r','\\','S','-','1','-','5','-','2','1','-','0','-','0','-','0','-','1','0','0','0','\\'
    static const WCHAR suppress_key[] = {HKCU_ROOT,'S','o','f','t','w','a','r','e','\\',
                                         'C','r','o','s','s','O','v','e','r','\\',
                                         'S','u','p','p','r','e','s','s','A','l','t','L','o','a','d','e','r',0};
    static const WCHAR use_key[] = {HKCU_ROOT,'S','o','f','t','w','a','r','e','\\',
                                    'C','r','o','s','s','O','v','e','r','\\',
                                    'U','s','e','A','l','t','L','o','a','d','e','r',0};
    BOOL ret = FALSE;
    const char * HOSTPTR unixdir = NULL;  /* FIXME */
    const char * HOSTPTR socket_path;
    char * HOSTPTR * HOSTPTR argv;
    int sock = -1;
    struct sockaddr_un sa;
    ssize_t rc;
    uint32_t request_type;
    char * HOSTPTR * HOSTPTR elem;
    uint64_t length;
    uint8_t dummy;
    struct iovec iov;
    struct msghdr msg;
    struct {
        struct cmsghdr hdr;
        int fds[5]; // standard in, out, and error, wineserver socket, and WINE_WAIT_CHILD_PIPE
    } cmsg;
    int nullfd = -1;
    const char* HOSTPTR wait_child_pipe;
    uint32_t response;

    TRACE("loader %s wineserversocket %d stdin_fd %d stdout_fd %d unixdir %s winedebug %s wineloader %s\n",
          debugstr_a(loader), wineserversocket, stdin_fd, stdout_fd, debugstr_a(unixdir), debugstr_a(winedebug),
          debugstr_a(wineloader));

    socket_path = getenv("CX_ALT_LOADER_SOCKET");
    if (!socket_path)
    {
        TRACE("CX_ALT_LOADER_SOCKET is not set; nothing to do\n");
        goto failed;
    }

    TRACE("socket path %s\n", debugstr_a(socket_path));

    argv = build_argv( &params->CommandLine, 1 );
    if (argv[1])
    {
        const char * HOSTPTR exename, * HOSTPTR p;
        int len;
        WCHAR *exenameW;
        BOOL exists;

        exename = argv[1];
        if ((p = strrchr(exename, '/'))) exename = p + 1;
        if ((p = strrchr(exename, '\\'))) exename = p + 1;
        if (!(p = strrchr(exename, '.'))) p = exename + strlen(exename);

        exenameW = RtlAllocateHeap( GetProcessHeap(), 0, (p - exename + 1) * sizeof(WCHAR) );
        len = ntdll_umbstowcs(exename, p - exename, exenameW, p - exename);
        exenameW[len] = 0;

        if (has_key_value( exenameW, use_key, &exists ))
        {
            if (!exists)
            {
                TRACE("alternative loader skipped because exe name %s is not in whitelist (argv[1] %s)\n", debugstr_w(exenameW), debugstr_a(argv[1]));
                RtlFreeHeap( GetProcessHeap(), 0, exenameW );
                goto failed;
            }
        }
        else if (has_key_value( exenameW, suppress_key, &exists ))
        {
            if (exists)
            {
                TRACE("alternative loader suppressed for exe name %s (argv[1] %s)\n", debugstr_w(exenameW), debugstr_a(argv[1]));
                RtlFreeHeap( GetProcessHeap(), 0, exenameW );
                goto failed;
            }
        }
        RtlFreeHeap( GetProcessHeap(), 0, exenameW );
    }

    sa.sun_family = AF_UNIX;
    if (strlen(socket_path) >= sizeof(sa.sun_path))
    {
        WARN("socket path %s is too long for sockaddr_un\n", debugstr_a(socket_path));
        goto failed;
    }
    lstrcpynA(sa.sun_path, socket_path, sizeof(sa.sun_path));
#ifdef HAVE_STRUCT_SOCKADDR_UN_SUN_LEN
    sa.sun_len = SUN_LEN(&sa) + 1;
#endif

    unsetenv("CX_ALT_LOADER_SOCKET");
    socket_path = NULL;

    sock = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (sock == -1)
    {
        WARN("failed to create socket; errno %d\n", errno);
        goto failed;
    }

    if (fcntl(sock, F_SETFD, FD_CLOEXEC))
        WARN("failed to set socket close-on-exec; proceeding anyway\n");

    do
    {
        rc = connect(sock, (struct sockaddr*)&sa, sizeof(sa));
    } while (rc == -1 && errno == EINTR);
    if (rc)
    {
        WARN("failed to connect; errno %d\n", errno);
        goto failed;
    }

    if (is_64bit_arch( pe_info->cpu ))
        request_type = REQUEST_LOAD_WINE_64BIT;
    else
        request_type = REQUEST_LOAD_WINE;
    if (!write_data(sock, &request_type, sizeof(request_type)))
    {
        WARN("failed to write request type\n");
        goto failed;
    }

    if (!write_length_prefixed_buffer(sock, unixdir, unixdir ? strlen(unixdir) + 1 : 0))
    {
        WARN("failed to write working directory\n");
        goto failed;
    }

    length = 0;
    for (elem = main_envp; *elem; elem++) {
        // Skip a preexisting WINELOADER if we have a replacement
        if (wineloader && !strncmp(*elem, "WINELOADER=", 11))
            continue;
        else
            length += strlen(*elem) + 1;
    }
    if (winedebug) length += strlen(winedebug) + 1;
    if (wineloader) length += strlen(wineloader) + 1;

    if (!write_data(sock, &length, sizeof(length)))
    {
        WARN("failed to write environment length\n");
        goto failed;
    }

    for (elem = main_envp; *elem; elem++)
    {
        // Skip a preexisting WINELOADER if we have a replacement
        if (wineloader && !strncmp(*elem, "WINELOADER=", 11))
            continue;

        if (!write_data(sock, *elem, strlen(*elem) + 1))
        {
            WARN("failed to write environment variable\n");
            goto failed;
        }
    }
    if (winedebug && !write_data(sock, winedebug, strlen(winedebug) + 1))
    {
        WARN("failed to write environment variable\n");
        goto failed;
    }
    if (wineloader && !write_data(sock, wineloader, strlen(wineloader) + 1))
    {
        WARN("failed to write environment variable\n");
        goto failed;
    }

    length = 0;
    if (!loader) loader = ""; /* use empty string as placeholder */
    length += strlen(loader) + 1;
    for (elem = &argv[1]; *elem; elem++)
    {
        TRACE("argv[%d] %s\n", elem - argv, debugstr_a(*elem));
        length += strlen(*elem) + 1;
    }

    if (!write_data(sock, &length, sizeof(length)))
    {
        WARN("failed to write args length\n");
        goto failed;
    }

    if (!write_data(sock, loader, strlen(loader) + 1))
    {
        WARN("failed to write argv[0]\n");
        goto failed;
    }
    for (elem = &argv[1]; *elem; elem++)
    {
        if (!write_data(sock, *elem, strlen(*elem) + 1))
        {
            WARN("failed to write argument\n");
            goto failed;
        }
    }

    if (params->ConsoleFlags || params->ConsoleHandle == (HANDLE)1 ||
        (params->hStdInput == INVALID_HANDLE_VALUE && params->hStdOutput == INVALID_HANDLE_VALUE))
    {
        nullfd = open("/dev/null", O_RDWR);
        stdin_fd = nullfd;
        stdout_fd = nullfd;
    }

    iov.iov_base = &dummy;
    iov.iov_len = sizeof(dummy);

    cmsg.hdr.cmsg_level = SOL_SOCKET;
    cmsg.hdr.cmsg_type = SCM_RIGHTS;
    cmsg.fds[0] = stdin_fd == -1 ? 0 : stdin_fd;
    cmsg.fds[1] = stdout_fd == -1 ? 1 : stdout_fd;
    cmsg.fds[2] = 2;
    cmsg.fds[3] = wineserversocket;
    if ((wait_child_pipe = getenv("WINE_WAIT_CHILD_PIPE")) &&
        (cmsg.fds[4] = atoi(wait_child_pipe)))
        cmsg.hdr.cmsg_len = (char*)&cmsg.fds[5] - (char*)&cmsg;
    else
        cmsg.hdr.cmsg_len = (char*)&cmsg.fds[4] - (char*)&cmsg;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = &cmsg;
    msg.msg_controllen = cmsg.hdr.cmsg_len;
    msg.msg_flags = 0;

    do
    {
        rc = sendmsg(sock, &msg, 0);
    } while (rc == -1 && errno == EINTR);
    if (rc != 1)
    {
        WARN("failed to send file descriptors; errno %d\n", errno);
        goto failed;
    }

    if (shutdown(sock, SHUT_WR))
    {
        WARN("failed to shutdown socket for writing; errno %d\n", errno);
        goto failed;
    }

    if (!read_data(sock, &response, sizeof(response)))
    {
        WARN("failed to read response; errno %d\n", errno);
        goto failed;
    }

    ret = (response == RESPONSE_SUCCESS);

failed:
    if (sock != -1) close(sock);
    if (nullfd != -1) close(nullfd);
    return ret;
}

/***********************************************************************
 *           get_alternate_loader
 *
 * Get the name of the alternate (32 or 64 bit) Wine loader.
 */
static const char * HOSTPTR get_alternate_loader( char * HOSTPTR * HOSTPTR ret_env )
{
    char * HOSTPTR env;
    const char * HOSTPTR loader = NULL;
    const char * HOSTPTR loader_env = getenv( "WINELOADER" );

    *ret_env = NULL;

    if (loader_env)
    {
        int len = strlen( loader_env );
        if (!wine_is_64bit())
        {
            if (!(env = malloc( sizeof("WINELOADER=") + len + 2 ))) return NULL;
            strcpy( env, "WINELOADER=" );
            strcat( env, loader_env );
            len += sizeof("WINELOADER=") - 1;
            if (!strcmp( env + len - 6, "32on64" )) env[len - 6] = 0;
            strcat( env, "64" );
        }
        else
        {
            if (!(env = malloc( sizeof("WINELOADER=") + len + 6 ))) return NULL;
            strcpy( env, "WINELOADER=" );
            strcat( env, loader_env );
            len += sizeof("WINELOADER=") - 1;
            if (!strcmp( env + len - 2, "64" )) env[len - 2] = 0;
            if (wine_needs_32on64())
                strcat( env, "32on64" );
        }
        if (!loader)
        {
            if ((loader = strrchr( env, '/' ))) loader++;
            else loader = env;
        }
        *ret_env = env;
    }
    if (!loader) loader = wine_is_64bit() ? wine_needs_32on64() ? "wine32on64" : "wine" : "wine64";
    return loader;
}


static char * HOSTPTR * HOSTPTR build_argv( const UNICODE_STRING *cmdline, int reserved )
{
    char * HOSTPTR * HOSTPTR argv, * HOSTPTR arg, * HOSTPTR src, * HOSTPTR dst;
    int argc, in_quotes = 0, bcount = 0, len = cmdline->Length / sizeof(WCHAR);

    if (!(src = malloc( len * 3 + 1 ))) return NULL;
    len = ntdll_wcstoumbs( cmdline->Buffer, len, src, len * 3, FALSE );
    src[len++] = 0;

    argc = reserved + 2 + len / 2;
    argv = malloc( argc * sizeof(*argv) + len );
    arg = dst = (char * HOSTPTR)(argv + argc);
    argc = reserved;
    while (*src)
    {
        if ((*src == ' ' || *src == '\t') && !in_quotes)
        {
            /* skip the remaining spaces */
            while (*src == ' ' || *src == '\t') src++;
            if (!*src) break;
            /* close the argument and copy it */
            *dst++ = 0;
            argv[argc++] = arg;
            /* start with a new argument */
            arg = dst;
            bcount = 0;
        }
        else if (*src == '\\')
        {
            *dst++ = *src++;
            bcount++;
        }
        else if (*src == '"')
        {
            if ((bcount & 1) == 0)
            {
                /* Preceded by an even number of '\', this is half that
                 * number of '\', plus a '"' which we discard.
                 */
                dst -= bcount / 2;
                src++;
                if (in_quotes && *src == '"') *dst++ = *src++;
                else in_quotes = !in_quotes;
            }
            else
            {
                /* Preceded by an odd number of '\', this is half that
                 * number of '\' followed by a '"'
                 */
                dst -= bcount / 2 + 1;
                *dst++ = *src++;
            }
            bcount = 0;
        }
        else  /* a regular character */
        {
            *dst++ = *src++;
            bcount = 0;
        }
    }
    *dst = 0;
    argv[argc++] = arg;
    argv[argc] = NULL;
    return argv;
}


#ifdef __APPLE__
/***********************************************************************
 *           terminate_main_thread
 *
 * On some versions of Mac OS X, the execve system call fails with
 * ENOTSUP if the process has multiple threads.  Wine is always multi-
 * threaded on Mac OS X because it specifically reserves the main thread
 * for use by the system frameworks (see apple_main_thread() in
 * libs/wine/loader.c).  So, when we need to exec without first forking,
 * we need to terminate the main thread first.  We do this by installing
 * a custom run loop source onto the main run loop and signaling it.
 * The source's "perform" callback is pthread_exit and it will be
 * executed on the main thread, terminating it.
 *
 * Returns TRUE if there's still hope the main thread has terminated or
 * will soon.  Return FALSE if we've given up.
 */
static BOOL terminate_main_thread(void)
{
    static int delayms;

    if (!delayms)
    {
        CFRunLoopSourceContext source_context = { 0 };
        CFRunLoopSourceRef source;

        source_context.perform = pthread_exit;
        if (!(source = CFRunLoopSourceCreate( NULL, 0, &source_context )))
            return FALSE;

        CFRunLoopAddSource( CFRunLoopGetMain(), source, kCFRunLoopCommonModes );
        CFRunLoopSourceSignal( source );
        CFRunLoopWakeUp( CFRunLoopGetMain() );
        CFRelease( source );

        delayms = 20;
    }

    if (delayms > 1000)
        return FALSE;

    usleep(delayms * 1000);
    delayms *= 2;

    return TRUE;
}
#endif


static inline const WCHAR *get_params_string( const RTL_USER_PROCESS_PARAMETERS *params,
                                              const UNICODE_STRING *str )
{
    if (params->Flags & PROCESS_PARAMS_FLAG_NORMALIZED) return str->Buffer;
    return (const WCHAR *)((const char *)params + (UINT_PTR)str->Buffer);
}

static inline DWORD append_string( void * HOSTPTR * HOSTPTR ptr, const RTL_USER_PROCESS_PARAMETERS *params,
                                   const UNICODE_STRING *str )
{
    const WCHAR *buffer = get_params_string( params, str );
    memcpy( *ptr, buffer, str->Length );
    *ptr = (WCHAR * HOSTPTR)*ptr + str->Length / sizeof(WCHAR);
    return str->Length;
}

/***********************************************************************
 *           create_startup_info
 */
static startup_info_t * HOSTPTR create_startup_info( const RTL_USER_PROCESS_PARAMETERS *params, DWORD *info_size )
{
    startup_info_t * HOSTPTR info;
    DWORD size;
    void * HOSTPTR ptr;

    size = sizeof(*info);
    size += params->CurrentDirectory.DosPath.Length;
    size += params->DllPath.Length;
    size += params->ImagePathName.Length;
    size += params->CommandLine.Length;
    size += params->WindowTitle.Length;
    size += params->Desktop.Length;
    size += params->ShellInfo.Length;
    size += params->RuntimeInfo.Length;
    size = (size + 1) & ~1;
    *info_size = size;

    if (!(info = calloc( size, 1 ))) return NULL;

    info->debug_flags   = params->DebugFlags;
    info->console_flags = params->ConsoleFlags;
    info->console       = wine_server_obj_handle( params->ConsoleHandle );
    info->hstdin        = wine_server_obj_handle( params->hStdInput );
    info->hstdout       = wine_server_obj_handle( params->hStdOutput );
    info->hstderr       = wine_server_obj_handle( params->hStdError );
    info->x             = params->dwX;
    info->y             = params->dwY;
    info->xsize         = params->dwXSize;
    info->ysize         = params->dwYSize;
    info->xchars        = params->dwXCountChars;
    info->ychars        = params->dwYCountChars;
    info->attribute     = params->dwFillAttribute;
    info->flags         = params->dwFlags;
    info->show          = params->wShowWindow;

    ptr = info + 1;
    info->curdir_len = append_string( &ptr, params, &params->CurrentDirectory.DosPath );
    info->dllpath_len = append_string( &ptr, params, &params->DllPath );
    info->imagepath_len = append_string( &ptr, params, &params->ImagePathName );
    info->cmdline_len = append_string( &ptr, params, &params->CommandLine );
    info->title_len = append_string( &ptr, params, &params->WindowTitle );
    info->desktop_len = append_string( &ptr, params, &params->Desktop );
    info->shellinfo_len = append_string( &ptr, params, &params->ShellInfo );
    info->runtime_len = append_string( &ptr, params, &params->RuntimeInfo );
    return info;
}


/***************************************************************************
 *	is_builtin_path
 */
static BOOL is_builtin_path( UNICODE_STRING *path, BOOL *is_64bit )
{
    static const WCHAR systemW[] = {'\\','?','?','\\','c',':','\\','w','i','n','d','o','w','s','\\',
                                    's','y','s','t','e','m','3','2','\\'};
    static const WCHAR wow64W[] = {'\\','?','?','\\','c',':','\\','w','i','n','d','o','w','s','\\',
                                   's','y','s','w','o','w','6','4'};

    *is_64bit = wine_is_64bit();
    if (path->Length > sizeof(systemW) && !wcsnicmp( path->Buffer, systemW, ARRAY_SIZE(systemW) ))
    {
#ifndef _WIN64
        if (NtCurrentTeb64() && NtCurrentTeb64()->TlsSlots[WOW64_TLS_FILESYSREDIR]) *is_64bit = TRUE;
#endif
        return TRUE;
    }
    if ((wine_is_64bit() || is_wow64) && path->Length > sizeof(wow64W) &&
        !wcsnicmp( path->Buffer, wow64W, ARRAY_SIZE(wow64W) ))
    {
        *is_64bit = FALSE;
        return TRUE;
    }
    return FALSE;
}


/***********************************************************************
 *           get_so_file_info
 */
static BOOL get_so_file_info( HANDLE handle, pe_image_info_t *info )
{
    union
    {
        struct
        {
            unsigned char magic[4];
            unsigned char class;
            unsigned char data;
            unsigned char version;
            unsigned char ignored1[9];
            unsigned short type;
            unsigned short machine;
            unsigned char ignored2[8];
            unsigned int phoff;
            unsigned char ignored3[12];
            unsigned short phnum;
        } elf;
        struct
        {
            unsigned char magic[4];
            unsigned char class;
            unsigned char data;
            unsigned char ignored1[10];
            unsigned short type;
            unsigned short machine;
            unsigned char ignored2[12];
            unsigned __int64 phoff;
            unsigned char ignored3[16];
            unsigned short phnum;
        } elf64;
        struct
        {
            unsigned int magic;
            unsigned int cputype;
            unsigned int cpusubtype;
            unsigned int filetype;
            unsigned int ncmds;
            unsigned int sizeofcmds;
            unsigned int flags;
            unsigned int reserved;
        } macho;
        IMAGE_DOS_HEADER mz;
    } header;

    IO_STATUS_BLOCK io;
    LARGE_INTEGER offset;

    offset.QuadPart = 0;
    if (NtReadFile( handle, 0, NULL, NULL, &io, &header, sizeof(header), &offset, 0 )) return FALSE;
    if (io.Information != sizeof(header)) return FALSE;

    if (!memcmp( header.elf.magic, "\177ELF", 4 ))
    {
        unsigned int type;
        unsigned short phnum;

        if (header.elf.version != 1 /* EV_CURRENT */) return FALSE;
#ifdef WORDS_BIGENDIAN
        if (header.elf.data != 2 /* ELFDATA2MSB */) return FALSE;
#else
        if (header.elf.data != 1 /* ELFDATA2LSB */) return FALSE;
#endif
        switch (header.elf.machine)
        {
        case 3:   info->cpu = CPU_x86; break;
        case 40:  info->cpu = CPU_ARM; break;
        case 62:  info->cpu = CPU_x86_64; break;
        case 183: info->cpu = CPU_ARM64; break;
        }
        if (header.elf.type != 3 /* ET_DYN */) return FALSE;
        if (header.elf.class == 2 /* ELFCLASS64 */)
        {
            offset.QuadPart = header.elf64.phoff;
            phnum = header.elf64.phnum;
        }
        else
        {
            offset.QuadPart = header.elf.phoff;
            phnum = header.elf.phnum;
        }
        while (phnum--)
        {
            if (NtReadFile( handle, 0, NULL, NULL, &io, &type, sizeof(type), &offset, 0 )) return FALSE;
            if (io.Information < sizeof(type)) return FALSE;
            if (type == 3 /* PT_INTERP */) return FALSE;
            offset.QuadPart += (header.elf.class == 2) ? 56 : 32;
        }
        return TRUE;
    }
    else if (header.macho.magic == 0xfeedface || header.macho.magic == 0xfeedfacf)
    {
        switch (header.macho.cputype)
        {
        case 0x00000007: info->cpu = CPU_x86; break;
        case 0x01000007: info->cpu = CPU_x86_64; break;
        case 0x0000000c: info->cpu = CPU_ARM; break;
        case 0x0100000c: info->cpu = CPU_ARM64; break;
        }
        if (header.macho.magic == 0xfeedfacf && info->cpu == CPU_x86_64)
        {
            struct segment_command_64
            {
                unsigned int    cmd;
                unsigned int    cmdsize;
                char            segname[16];
            } seg;

            offset.QuadPart = sizeof(header.macho);

            while (header.macho.ncmds--)
            {
                if (NtReadFile( handle, 0, NULL, NULL, &io, &seg, sizeof(seg), &offset, 0 ) ||
                    io.Information < sizeof(seg))
                    break;
                if (seg.cmd == 0x19 /*LC_SEGMENT_64*/ && !strcmp(seg.segname, "WINE_32on64"))
                {
                    info->cpu = CPU_x86;
                    break;
                }
                offset.QuadPart += seg.cmdsize;
            }
        }
        if (header.macho.filetype == 8) return TRUE;
    }
    return FALSE;
}


/***********************************************************************
 *           get_pe_file_info
 */
static NTSTATUS get_pe_file_info( UNICODE_STRING *path, HANDLE *handle, pe_image_info_t *info )
{
    NTSTATUS status;
    HANDLE mapping;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;

    *handle = 0;
    memset( info, 0, sizeof(*info) );
    InitializeObjectAttributes( &attr, path, OBJ_CASE_INSENSITIVE, 0, 0 );
    if ((status = NtOpenFile( handle, GENERIC_READ, &attr, &io,
                              FILE_SHARE_READ | FILE_SHARE_DELETE, FILE_SYNCHRONOUS_IO_NONALERT )))
    {
        BOOL is_64bit;

        if (is_builtin_path( path, &is_64bit ))
        {
            TRACE( "assuming %u-bit builtin for %s\n", is_64bit ? 64 : 32, debugstr_us(path));
            /* assume current arch */
#if defined(__i386__) || defined(__x86_64__) || defined(__i386_on_x86_64__)
            info->cpu = is_64bit ? CPU_x86_64 : CPU_x86;
#else
            info->cpu = client_cpu;
#endif
            return STATUS_SUCCESS;
        }
        return status;
    }

    if (!(status = NtCreateSection( &mapping, STANDARD_RIGHTS_REQUIRED | SECTION_QUERY |
                                    SECTION_MAP_READ | SECTION_MAP_EXECUTE,
                                    NULL, NULL, PAGE_EXECUTE_READ, SEC_IMAGE, *handle )))
    {
        SERVER_START_REQ( get_mapping_info )
        {
            req->handle = wine_server_obj_handle( mapping );
            req->access = SECTION_QUERY;
            wine_server_set_reply( req, info, sizeof(*info) );
            status = wine_server_call( req );
        }
        SERVER_END_REQ;
        NtClose( mapping );
    }
    else if (status == STATUS_INVALID_IMAGE_NOT_MZ)
    {
        if (get_so_file_info( *handle, info )) return STATUS_SUCCESS;
    }
    return status;
}


/***********************************************************************
 *           get_env_size
 */
static ULONG get_env_size( const RTL_USER_PROCESS_PARAMETERS *params, char * HOSTPTR *winedebug )
{
    WCHAR *ptr = params->Environment;

    while (*ptr)
    {
        static const WCHAR WINEDEBUG[] = {'W','I','N','E','D','E','B','U','G','=',0};
        if (!*winedebug && !wcsncmp( ptr, WINEDEBUG, ARRAY_SIZE( WINEDEBUG ) - 1 ))
        {
            DWORD len = wcslen(ptr) * 3 + 1;
            if ((*winedebug = malloc( len )))
                ntdll_wcstoumbs( ptr, wcslen(ptr) + 1, *winedebug, len, FALSE );
        }
        ptr += wcslen(ptr) + 1;
    }
    ptr++;
    return (ptr - params->Environment) * sizeof(WCHAR);
}


/***********************************************************************
 *           get_nt_pathname
 *
 * Simplified version of RtlDosPathNameToNtPathName_U.
 */
static WCHAR *get_nt_pathname( const UNICODE_STRING *str )
{
    static const WCHAR ntprefixW[] = {'\\','?','?','\\',0};
    static const WCHAR uncprefixW[] = {'U','N','C','\\',0};
    const WCHAR *name = str->Buffer;
    WCHAR *ret;

    if (!(ret = RtlAllocateHeap( GetProcessHeap(), 0, str->Length + 8 * sizeof(WCHAR) ))) return NULL;

    wcscpy( ret, ntprefixW );
    if (name[0] == '\\' && name[1] == '\\')
    {
        if ((name[2] == '.' || name[2] == '?') && name[3] == '\\') name += 4;
        else
        {
            wcscat( ret, uncprefixW );
            name += 2;
        }
    }
    wcscat( ret, name );
    return ret;
}


/***********************************************************************
 *           get_unix_curdir
 */
static int get_unix_curdir( const RTL_USER_PROCESS_PARAMETERS *params )
{
    UNICODE_STRING nt_name;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    HANDLE handle;
    int fd = -1;

    if (!(nt_name.Buffer = get_nt_pathname( &params->CurrentDirectory.DosPath ))) return -1;
    nt_name.Length = wcslen( nt_name.Buffer ) * sizeof(WCHAR);

    InitializeObjectAttributes( &attr, &nt_name, OBJ_CASE_INSENSITIVE, 0, NULL );
    status = NtOpenFile( &handle, FILE_TRAVERSE | SYNCHRONIZE, &attr, &io,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT );
    RtlFreeHeap( GetProcessHeap(), 0, nt_name.Buffer );
    if (status) return -1;
    wine_server_handle_to_fd( handle, FILE_TRAVERSE, &fd, NULL );
    NtClose( handle );
    return fd;
}


/***********************************************************************
 *           set_stdio_fd
 */
static void set_stdio_fd( int stdin_fd, int stdout_fd )
{
    int fd = -1;

    if (stdin_fd == -1 || stdout_fd == -1)
    {
        fd = open( "/dev/null", O_RDWR );
        if (stdin_fd == -1) stdin_fd = fd;
        if (stdout_fd == -1) stdout_fd = fd;
    }

    dup2( stdin_fd, 0 );
    dup2( stdout_fd, 1 );
    if (fd != -1) close( fd );
}


/***********************************************************************
 *           spawn_process
 */
static NTSTATUS spawn_process( const RTL_USER_PROCESS_PARAMETERS *params, int socketfd,
                               int unixdir, char * HOSTPTR winedebug, const pe_image_info_t *pe_info )
{
    int is_child_64bit = (pe_info->cpu == CPU_x86_64 || pe_info->cpu == CPU_ARM64);
    NTSTATUS status = STATUS_SUCCESS;
    int stdin_fd = -1, stdout_fd = -1;
    pid_t pid;
    char * HOSTPTR * HOSTPTR argv;
    char * HOSTPTR wineloader = NULL;
    const char * HOSTPTR loader = NULL;

    if (!wine_is_64bit() ^ !is_child_64bit) loader = get_alternate_loader( &wineloader );

    wine_server_handle_to_fd( params->hStdInput, FILE_READ_DATA, &stdin_fd, NULL );
    wine_server_handle_to_fd( params->hStdOutput, FILE_WRITE_DATA, &stdout_fd, NULL );

    /* CrossOver Hack 10523: shunt the loading to CrossOver */
    if (send_to_cx_loader(loader, params, socketfd, stdin_fd, stdout_fd,
                          winedebug, pe_info, wineloader))
        goto done;

    if (!(pid = fork()))  /* child */
    {
        if (!(pid = fork()))  /* grandchild */
        {
            if (params->ConsoleFlags ||
                params->ConsoleHandle == (HANDLE)1 /* KERNEL32_CONSOLE_ALLOC */ ||
                (params->hStdInput == INVALID_HANDLE_VALUE && params->hStdOutput == INVALID_HANDLE_VALUE))
            {
                setsid();
                set_stdio_fd( -1, -1 );  /* close stdin and stdout */
            }
            else set_stdio_fd( stdin_fd, stdout_fd );

            if (stdin_fd != -1) close( stdin_fd );
            if (stdout_fd != -1) close( stdout_fd );

            if (winedebug) putenv( winedebug );
            if (unixdir != -1)
            {
                fchdir( unixdir );
                close( unixdir );
            }
            argv = build_argv( &params->CommandLine, 2 );

            exec_wineloader( argv, socketfd, pe_info );
            _exit(1);
        }

        _exit(pid == -1);
    }

    if (pid != -1)
    {
        /* reap child */
        pid_t wret;
        do {
            wret = waitpid(pid, NULL, 0);
        } while (wret < 0 && errno == EINTR);
    }
    else status = STATUS_NO_MEMORY;

done: /* CrossOver Hack 10523 */
    free( wineloader );
    if (stdin_fd != -1) close( stdin_fd );
    if (stdout_fd != -1) close( stdout_fd );
    return status;
}


/***********************************************************************
 *           exec_process
 */
void DECLSPEC_NORETURN exec_process( NTSTATUS status )
{
    RTL_USER_PROCESS_PARAMETERS *params = NtCurrentTeb()->Peb->ProcessParameters;
    pe_image_info_t pe_info;
    int unixdir, socketfd[2];
    char * HOSTPTR * HOSTPTR argv;
    HANDLE handle;

    if (startup_info_size) goto done;  /* started from another Win32 process */

    switch (status)
    {
    case STATUS_CONFLICTING_ADDRESSES:
    case STATUS_NO_MEMORY:
    case STATUS_INVALID_IMAGE_FORMAT:
    case STATUS_INVALID_IMAGE_NOT_MZ:
    {
        UNICODE_STRING image;
        if (getenv( "WINEPRELOADRESERVE" )) goto done;
        image.Buffer = get_nt_pathname( &params->ImagePathName );
        image.Length = wcslen( image.Buffer ) * sizeof(WCHAR);
        if ((status = get_pe_file_info( &image, &handle, &pe_info ))) goto done;
        break;
    }
    case STATUS_INVALID_IMAGE_WIN_16:
    case STATUS_INVALID_IMAGE_NE_FORMAT:
    case STATUS_INVALID_IMAGE_PROTECT:
        /* we'll start winevdm */
        memset( &pe_info, 0, sizeof(pe_info) );
        pe_info.cpu = CPU_x86;
        break;
    default:
        goto done;
    }

    unixdir = get_unix_curdir( params );

    if (socketpair( PF_UNIX, SOCK_STREAM, 0, socketfd ) == -1)
    {
        status = STATUS_TOO_MANY_OPENED_FILES;
        goto done;
    }
#ifdef SO_PASSCRED
    else
    {
        int enable = 1;
        setsockopt( socketfd[0], SOL_SOCKET, SO_PASSCRED, &enable, sizeof(enable) );
    }
#endif
    wine_server_send_fd( socketfd[1] );
    close( socketfd[1] );

    SERVER_START_REQ( exec_process )
    {
        req->socket_fd = socketfd[1];
        req->cpu       = pe_info.cpu;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;

    if (!status)
    {
        if (!(argv = build_argv( &params->CommandLine, 2 )))
        {
            status = STATUS_NO_MEMORY;
            goto done;
        }
        fchdir( unixdir );
        do
        {
            status = exec_wineloader( argv, socketfd[0], &pe_info );
        }
#ifdef __APPLE__
        while (errno == ENOTSUP && terminate_main_thread());
#else
        while (0);
#endif
        free( argv );
    }
    close( socketfd[0] );

done:
    switch (status)
    {
    case STATUS_INVALID_IMAGE_FORMAT:
    case STATUS_INVALID_IMAGE_NOT_MZ:
        ERR( "%s not supported on this system\n", debugstr_us(&params->ImagePathName) );
        break;
    default:
        ERR( "failed to load %s error %x\n", debugstr_us(&params->ImagePathName), status );
        break;
    }
    for (;;) NtTerminateProcess( GetCurrentProcess(), status );
}


/***********************************************************************
 *           fork_and_exec
 *
 * Fork and exec a new Unix binary, checking for errors.
 */
static NTSTATUS fork_and_exec( UNICODE_STRING *path, int unixdir,
                               const RTL_USER_PROCESS_PARAMETERS *params )
{
    pid_t pid;
    int fd[2], stdin_fd = -1, stdout_fd = -1;
    char * HOSTPTR * HOSTPTR argv, * HOSTPTR * HOSTPTR envp;
    char * HOSTPTR unix_name;
    NTSTATUS status;

    status = nt_to_unix_file_name( path, &unix_name, FILE_OPEN );
    if (status) return status;

#ifdef HAVE_PIPE2
    if (pipe2( fd, O_CLOEXEC ) == -1)
#endif
    {
        if (pipe(fd) == -1)
        {
            status = STATUS_TOO_MANY_OPENED_FILES;
            goto done;
        }
        fcntl( fd[0], F_SETFD, FD_CLOEXEC );
        fcntl( fd[1], F_SETFD, FD_CLOEXEC );
    }

    wine_server_handle_to_fd( params->hStdInput, FILE_READ_DATA, &stdin_fd, NULL );
    wine_server_handle_to_fd( params->hStdOutput, FILE_WRITE_DATA, &stdout_fd, NULL );

    if (!(pid = fork()))  /* child */
    {
        if (!(pid = fork()))  /* grandchild */
        {
            close( fd[0] );

            if (params->ConsoleFlags ||
                params->ConsoleHandle == (HANDLE)1 /* KERNEL32_CONSOLE_ALLOC */ ||
                (params->hStdInput == INVALID_HANDLE_VALUE && params->hStdOutput == INVALID_HANDLE_VALUE))
            {
                setsid();
                set_stdio_fd( -1, -1 );  /* close stdin and stdout */
            }
            else set_stdio_fd( stdin_fd, stdout_fd );

            if (stdin_fd != -1) close( stdin_fd );
            if (stdout_fd != -1) close( stdout_fd );

            /* Reset signals that we previously set to SIG_IGN */
            signal( SIGPIPE, SIG_DFL );

            argv = build_argv( &params->CommandLine, 0 );
            envp = build_envp( params->Environment );
            if (unixdir != -1)
            {
                fchdir( unixdir );
                close( unixdir );
            }
            execve( unix_name, argv, envp );
        }

        if (pid <= 0)  /* grandchild if exec failed or child if fork failed */
        {
            switch (errno)
            {
            case EPERM:
            case EACCES: status = STATUS_ACCESS_DENIED; break;
            case ENOENT: status = STATUS_OBJECT_NAME_NOT_FOUND; break;
            case EMFILE:
            case ENFILE: status = STATUS_TOO_MANY_OPENED_FILES; break;
            case ENOEXEC:
            case EINVAL: status = STATUS_INVALID_IMAGE_FORMAT; break;
            default:     status = STATUS_NO_MEMORY; break;
            }
            write( fd[1], &status, sizeof(status) );
            _exit(1);
        }
        _exit(0); /* child if fork succeeded */
    }
    close( fd[1] );

    if (pid != -1)
    {
        /* reap child */
        pid_t wret;
        do {
            wret = waitpid(pid, NULL, 0);
        } while (wret < 0 && errno == EINTR);
        read( fd[0], &status, sizeof(status) );  /* if we read something, exec or second fork failed */
    }
    else status = STATUS_NO_MEMORY;

    close( fd[0] );
    if (stdin_fd != -1) close( stdin_fd );
    if (stdout_fd != -1) close( stdout_fd );
done:
    free( unix_name );
    return status;
}

static NTSTATUS alloc_handle_list( const PS_ATTRIBUTE *handles_attr, obj_handle_t * HOSTPTR * HOSTPTR handles, data_size_t *handles_len )
{
    SIZE_T count, i;
    HANDLE *src;

    *handles = NULL;
    *handles_len = 0;

    if (!handles_attr) return STATUS_SUCCESS;

    count = handles_attr->Size / sizeof(HANDLE);

    if (!(*handles = calloc( sizeof(**handles), count ))) return STATUS_NO_MEMORY;

    src = handles_attr->ValuePtr;
    for (i = 0; i < count; ++i)
        (*handles)[i] = wine_server_obj_handle( src[i] );

    *handles_len = count * sizeof(**handles);

    return STATUS_SUCCESS;
}

/**********************************************************************
 *           NtCreateUserProcess  (NTDLL.@)
 */
NTSTATUS WINAPI NtCreateUserProcess( HANDLE *process_handle_ptr, HANDLE *thread_handle_ptr,
                                     ACCESS_MASK process_access, ACCESS_MASK thread_access,
                                     OBJECT_ATTRIBUTES *process_attr, OBJECT_ATTRIBUTES *thread_attr,
                                     ULONG process_flags, ULONG thread_flags,
                                     RTL_USER_PROCESS_PARAMETERS *params, PS_CREATE_INFO *info,
                                     PS_ATTRIBUTE_LIST *attr )
{
    NTSTATUS status;
    BOOL success = FALSE;
    HANDLE file_handle, process_info = 0, process_handle = 0, thread_handle = 0;
    struct object_attributes * HOSTPTR objattr;
    data_size_t attr_len;
    char * HOSTPTR winedebug = NULL;
    startup_info_t * HOSTPTR startup_info = NULL;
    ULONG startup_info_size, env_size;
    int unixdir, socketfd[2] = { -1, -1 };
    pe_image_info_t pe_info;
    CLIENT_ID id;
    HANDLE parent = 0, debug = 0, token = 0;
    UNICODE_STRING path = {0};
    SIZE_T i, attr_count = (attr->TotalLength - sizeof(attr->TotalLength)) / sizeof(PS_ATTRIBUTE);
    const PS_ATTRIBUTE *handles_attr = NULL;
    data_size_t handles_size;
    obj_handle_t * HOSTPTR handles;

    for (i = 0; i < attr_count; i++)
    {
        switch (attr->Attributes[i].Attribute)
        {
        case PS_ATTRIBUTE_PARENT_PROCESS:
            parent = attr->Attributes[i].ValuePtr;
            break;
        case PS_ATTRIBUTE_DEBUG_PORT:
            debug = attr->Attributes[i].ValuePtr;
            break;
        case PS_ATTRIBUTE_IMAGE_NAME:
            path.Length = attr->Attributes[i].Size;
            path.Buffer = attr->Attributes[i].ValuePtr;
            break;
        case PS_ATTRIBUTE_TOKEN:
            token = attr->Attributes[i].ValuePtr;
            break;
        case PS_ATTRIBUTE_HANDLE_LIST:
            if (process_flags & PROCESS_CREATE_FLAGS_INHERIT_HANDLES)
                handles_attr = &attr->Attributes[i];
            break;
        default:
            if (attr->Attributes[i].Attribute & PS_ATTRIBUTE_INPUT)
                FIXME( "unhandled input attribute %lx\n", attr->Attributes[i].Attribute );
            break;
        }
    }

    TRACE( "%s image %s cmdline %s parent %p\n", debugstr_us( &path ),
           debugstr_us( &params->ImagePathName ), debugstr_us( &params->CommandLine ), parent );
    if (debug) FIXME( "debug port %p not supported yet\n", debug );

    unixdir = get_unix_curdir( params );

    if ((status = get_pe_file_info( &path, &file_handle, &pe_info )))
    {
        if (status == STATUS_INVALID_IMAGE_NOT_MZ && !fork_and_exec( &path, unixdir, params ))
        {
            memset( info, 0, sizeof(*info) );
            return STATUS_SUCCESS;
        }
        goto done;
    }
    if (!(startup_info = create_startup_info( params, &startup_info_size ))) goto done;
    env_size = get_env_size( params, &winedebug );

    if ((status = alloc_object_attributes( process_attr, &objattr, &attr_len ))) goto done;

    if ((status = alloc_handle_list( handles_attr, &handles, &handles_size )))
    {
        free( objattr );
        goto done;
    }

    /* create the socket for the new process */

    if (socketpair( PF_UNIX, SOCK_STREAM, 0, socketfd ) == -1)
    {
        status = STATUS_TOO_MANY_OPENED_FILES;
        free( objattr );
        free( handles );
        goto done;
    }
#ifdef SO_PASSCRED
    else
    {
        int enable = 1;
        setsockopt( socketfd[0], SOL_SOCKET, SO_PASSCRED, &enable, sizeof(enable) );
    }
#endif

    wine_server_send_fd( socketfd[1] );
    close( socketfd[1] );

    /* create the process on the server side */

    SERVER_START_REQ( new_process )
    {
        req->token          = wine_server_obj_handle( token );
        req->parent_process = wine_server_obj_handle( parent );
        req->inherit_all    = !!(process_flags & PROCESS_CREATE_FLAGS_INHERIT_HANDLES);
        req->create_flags   = params->DebugFlags; /* hack: creation flags stored in DebugFlags for now */
        req->socket_fd      = socketfd[1];
        req->exe_file       = wine_server_obj_handle( file_handle );
        req->access         = process_access;
        req->cpu            = pe_info.cpu;
        req->info_size      = startup_info_size;
        req->handles_size   = handles_size;
        wine_server_add_data( req, objattr, attr_len );
        wine_server_add_data( req, handles, handles_size );
        wine_server_add_data( req, startup_info, startup_info_size );
        wine_server_add_data( req, params->Environment, env_size );
        if (!(status = wine_server_call( req )))
        {
            process_handle = wine_server_ptr_handle( reply->handle );
            id.UniqueProcess = ULongToHandle( reply->pid );
        }
        process_info = wine_server_ptr_handle( reply->info );
    }
    SERVER_END_REQ;
    free( objattr );
    free( handles );

    if (status)
    {
        switch (status)
        {
        case STATUS_INVALID_IMAGE_WIN_64:
            ERR( "64-bit application %s not supported in 32-bit prefix\n", debugstr_us(&path) );
            break;
        case STATUS_INVALID_IMAGE_FORMAT:
            ERR( "%s not supported on this installation (%s binary)\n",
                 debugstr_us(&path), cpu_names[pe_info.cpu] );
            break;
        }
        goto done;
    }

    if ((status = alloc_object_attributes( thread_attr, &objattr, &attr_len ))) goto done;

    SERVER_START_REQ( new_thread )
    {
        req->process    = wine_server_obj_handle( process_handle );
        req->access     = thread_access;
        req->suspend    = !!(thread_flags & THREAD_CREATE_FLAGS_CREATE_SUSPENDED);
        req->request_fd = -1;
        wine_server_add_data( req, objattr, attr_len );
        if (!(status = wine_server_call( req )))
        {
            thread_handle = wine_server_ptr_handle( reply->handle );
            id.UniqueThread = ULongToHandle( reply->tid );
        }
    }
    SERVER_END_REQ;
    free( objattr );
    if (status) goto done;

    /* create the child process */

    if ((status = spawn_process( params, socketfd[0], unixdir, winedebug, &pe_info ))) goto done;

    close( socketfd[0] );
    socketfd[0] = -1;

    /* wait for the new process info to be ready */

    NtWaitForSingleObject( process_info, FALSE, NULL );
    SERVER_START_REQ( get_new_process_info )
    {
        req->info = wine_server_obj_handle( process_info );
        wine_server_call( req );
        success = reply->success;
        status = reply->exit_code;
    }
    SERVER_END_REQ;

    if (!success)
    {
        if (!status) status = STATUS_INTERNAL_ERROR;
        goto done;
    }

    TRACE( "%s pid %04x tid %04x handles %p/%p\n", debugstr_us(&path),
           HandleToULong(id.UniqueProcess), HandleToULong(id.UniqueThread),
           process_handle, thread_handle );

    /* update output attributes */

    for (i = 0; i < attr_count; i++)
    {
        switch (attr->Attributes[i].Attribute)
        {
        case PS_ATTRIBUTE_CLIENT_ID:
        {
            SIZE_T size = min( attr->Attributes[i].Size, sizeof(id) );
            memcpy( attr->Attributes[i].ValuePtr, &id, size );
            if (attr->Attributes[i].ReturnLength) *attr->Attributes[i].ReturnLength = size;
            break;
        }
        case PS_ATTRIBUTE_IMAGE_INFO:
        {
            SECTION_IMAGE_INFORMATION info;
            SIZE_T size = min( attr->Attributes[i].Size, sizeof(info) );
            virtual_fill_image_information( &pe_info, &info );
            memcpy( attr->Attributes[i].ValuePtr, &info, size );
            if (attr->Attributes[i].ReturnLength) *attr->Attributes[i].ReturnLength = size;
            break;
        }
        case PS_ATTRIBUTE_TEB_ADDRESS:
        default:
            if (!(attr->Attributes[i].Attribute & PS_ATTRIBUTE_INPUT))
                FIXME( "unhandled output attribute %lx\n", attr->Attributes[i].Attribute );
            break;
        }
    }
    *process_handle_ptr = process_handle;
    *thread_handle_ptr = thread_handle;
    process_handle = thread_handle = 0;
    status = STATUS_SUCCESS;

done:
    if (file_handle) NtClose( file_handle );
    if (process_info) NtClose( process_info );
    if (process_handle) NtClose( process_handle );
    if (thread_handle) NtClose( thread_handle );
    if (socketfd[0] != -1) close( socketfd[0] );
    if (unixdir != -1) close( unixdir );
    free( startup_info );
    free( winedebug );
    return status;
}


/******************************************************************************
 *              NtTerminateProcess  (NTDLL.@)
 */
NTSTATUS WINAPI NtTerminateProcess( HANDLE handle, LONG exit_code )
{
    NTSTATUS ret;
    BOOL self;

    SERVER_START_REQ( terminate_process )
    {
        req->handle    = wine_server_obj_handle( handle );
        req->exit_code = exit_code;
        ret = wine_server_call( req );
        self = reply->self;
    }
    SERVER_END_REQ;
    if (self)
    {
        if (!handle) process_exiting = TRUE;
        else if (process_exiting) exit_process( exit_code );
        else abort_process( exit_code );
    }
    return ret;
}


#if defined(HAVE_MACH_MACH_H)

void fill_vm_counters( VM_COUNTERS_EX *pvmi, int unix_pid )
{
#if defined(MACH_TASK_BASIC_INFO)
    struct mach_task_basic_info info;

    if (unix_pid != -1) return; /* FIXME: Retrieve information for other processes. */

    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if(task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) == KERN_SUCCESS)
    {
        pvmi->VirtualSize = info.resident_size + info.virtual_size;
        pvmi->PagefileUsage = info.virtual_size;
        pvmi->WorkingSetSize = info.resident_size;
        pvmi->PeakWorkingSetSize = info.resident_size_max;
    }
#endif
}

#elif defined(linux)

void fill_vm_counters( VM_COUNTERS_EX *pvmi, int unix_pid )
{
    FILE *f;
    char line[256], path[26];
    unsigned long value;

    if (unix_pid == -1)
        strcpy( path, "/proc/self/status" );
    else
        sprintf( path, "/proc/%u/status", unix_pid);
    f = fopen( path, "r" );
    if (!f) return;

    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "VmPeak: %lu", &value))
            pvmi->PeakVirtualSize = (ULONG64)value * 1024;
        else if (sscanf(line, "VmSize: %lu", &value))
            pvmi->VirtualSize = (ULONG64)value * 1024;
        else if (sscanf(line, "VmHWM: %lu", &value))
            pvmi->PeakWorkingSetSize = (ULONG64)value * 1024;
        else if (sscanf(line, "VmRSS: %lu", &value))
            pvmi->WorkingSetSize = (ULONG64)value * 1024;
        else if (sscanf(line, "RssAnon: %lu", &value))
            pvmi->PagefileUsage += (ULONG64)value * 1024;
        else if (sscanf(line, "VmSwap: %lu", &value))
            pvmi->PagefileUsage += (ULONG64)value * 1024;
    }
    pvmi->PeakPagefileUsage = pvmi->PagefileUsage;

    fclose(f);
}

#else

void fill_vm_counters( VM_COUNTERS_EX *pvmi, int unix_pid )
{
    /* FIXME : real data */
}

#endif

#define UNIMPLEMENTED_INFO_CLASS(c) \
    case c: \
        FIXME( "(process=%p) Unimplemented information class: " #c "\n", handle); \
        ret = STATUS_INVALID_INFO_CLASS; \
        break

/**********************************************************************
 *           NtQueryInformationProcess  (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryInformationProcess( HANDLE handle, PROCESSINFOCLASS class, void *info,
                                           ULONG size, ULONG *ret_len )
{
    NTSTATUS ret = STATUS_SUCCESS;
    ULONG len = 0;

    TRACE( "(%p,0x%08x,%p,0x%08x,%p)\n", handle, class, info, size, ret_len );

    switch (class)
    {
    UNIMPLEMENTED_INFO_CLASS(ProcessQuotaLimits);
    UNIMPLEMENTED_INFO_CLASS(ProcessBasePriority);
    UNIMPLEMENTED_INFO_CLASS(ProcessRaisePriority);
    UNIMPLEMENTED_INFO_CLASS(ProcessExceptionPort);
    UNIMPLEMENTED_INFO_CLASS(ProcessAccessToken);
    UNIMPLEMENTED_INFO_CLASS(ProcessLdtInformation);
    UNIMPLEMENTED_INFO_CLASS(ProcessLdtSize);
    UNIMPLEMENTED_INFO_CLASS(ProcessIoPortHandlers);
    UNIMPLEMENTED_INFO_CLASS(ProcessPooledUsageAndLimits);
    UNIMPLEMENTED_INFO_CLASS(ProcessWorkingSetWatch);
    UNIMPLEMENTED_INFO_CLASS(ProcessUserModeIOPL);
    UNIMPLEMENTED_INFO_CLASS(ProcessEnableAlignmentFaultFixup);
    UNIMPLEMENTED_INFO_CLASS(ProcessWx86Information);
    UNIMPLEMENTED_INFO_CLASS(ProcessPriorityBoost);
    UNIMPLEMENTED_INFO_CLASS(ProcessDeviceMap);
    UNIMPLEMENTED_INFO_CLASS(ProcessSessionInformation);
    UNIMPLEMENTED_INFO_CLASS(ProcessForegroundInformation);
    UNIMPLEMENTED_INFO_CLASS(ProcessLUIDDeviceMapsEnabled);
    UNIMPLEMENTED_INFO_CLASS(ProcessBreakOnTermination);
    UNIMPLEMENTED_INFO_CLASS(ProcessHandleTracing);

    case ProcessBasicInformation:
        {
            PROCESS_BASIC_INFORMATION pbi;
            const ULONG_PTR affinity_mask = get_system_affinity_mask();

            if (size >= sizeof(PROCESS_BASIC_INFORMATION))
            {
                if (!info) ret = STATUS_ACCESS_VIOLATION;
                else
                {
                    SERVER_START_REQ(get_process_info)
                    {
                        req->handle = wine_server_obj_handle( handle );
                        if ((ret = wine_server_call( req )) == STATUS_SUCCESS)
                        {
                            pbi.ExitStatus = reply->exit_code;
                            pbi.PebBaseAddress = wine_server_get_ptr( reply->peb );
                            pbi.AffinityMask = reply->affinity & affinity_mask;
                            pbi.BasePriority = reply->priority;
                            pbi.UniqueProcessId = reply->pid;
                            pbi.InheritedFromUniqueProcessId = reply->ppid;
                        }
                    }
                    SERVER_END_REQ;

                    memcpy( info, &pbi, sizeof(PROCESS_BASIC_INFORMATION) );
                    len = sizeof(PROCESS_BASIC_INFORMATION);
                }
                if (size > sizeof(PROCESS_BASIC_INFORMATION)) ret = STATUS_INFO_LENGTH_MISMATCH;
            }
            else
            {
                len = sizeof(PROCESS_BASIC_INFORMATION);
                ret = STATUS_INFO_LENGTH_MISMATCH;
            }
        }
        break;

    case ProcessIoCounters:
        {
            IO_COUNTERS pii;

            if (size >= sizeof(IO_COUNTERS))
            {
                if (!info) ret = STATUS_ACCESS_VIOLATION;
                else if (!handle) ret = STATUS_INVALID_HANDLE;
                else
                {
                    /* FIXME : real data */
                    memset(&pii, 0 , sizeof(IO_COUNTERS));
                    memcpy(info, &pii, sizeof(IO_COUNTERS));
                    len = sizeof(IO_COUNTERS);
                }
                if (size > sizeof(IO_COUNTERS)) ret = STATUS_INFO_LENGTH_MISMATCH;
            }
            else
            {
                len = sizeof(IO_COUNTERS);
                ret = STATUS_INFO_LENGTH_MISMATCH;
            }
        }
        break;

    case ProcessVmCounters:
        {
            VM_COUNTERS_EX pvmi;

            /* older Windows versions don't have the PrivateUsage field */
            if (size >= sizeof(VM_COUNTERS))
            {
                if (!info) ret = STATUS_ACCESS_VIOLATION;
                else
                {
                    memset(&pvmi, 0, sizeof(pvmi));
                    if (handle == GetCurrentProcess()) fill_vm_counters( &pvmi, -1 );
                    else
                    {
                        SERVER_START_REQ(get_process_vm_counters)
                        {
                            req->handle = wine_server_obj_handle( handle );
                            if (!(ret = wine_server_call( req )))
                            {
                                pvmi.PeakVirtualSize = reply->peak_virtual_size;
                                pvmi.VirtualSize = reply->virtual_size;
                                pvmi.PeakWorkingSetSize = reply->peak_working_set_size;
                                pvmi.WorkingSetSize = reply->working_set_size;
                                pvmi.PagefileUsage = reply->pagefile_usage;
                                pvmi.PeakPagefileUsage = reply->peak_pagefile_usage;
                            }
                        }
                        SERVER_END_REQ;
                        if (ret) break;
                    }
                    if (size >= sizeof(VM_COUNTERS_EX))
                        pvmi.PrivateUsage = pvmi.PagefileUsage;
                    len = size;
                    if (len != sizeof(VM_COUNTERS)) len = sizeof(VM_COUNTERS_EX);
                    memcpy(info, &pvmi, min(size, sizeof(pvmi)));
                }
                if (size != sizeof(VM_COUNTERS) && size != sizeof(VM_COUNTERS_EX))
                    ret = STATUS_INFO_LENGTH_MISMATCH;
            }
            else
            {
                len = sizeof(pvmi);
                ret = STATUS_INFO_LENGTH_MISMATCH;
            }
        }
        break;

    case ProcessTimes:
        {
            KERNEL_USER_TIMES pti = {{{0}}};

            if (size >= sizeof(KERNEL_USER_TIMES))
            {
                if (!info) ret = STATUS_ACCESS_VIOLATION;
                else if (!handle) ret = STATUS_INVALID_HANDLE;
                else
                {
                    long ticks = sysconf(_SC_CLK_TCK);
                    struct tms tms;

                    /* FIXME: user/kernel times only work for current process */
                    if (ticks && times( &tms ) != -1)
                    {
                        pti.UserTime.QuadPart = (ULONGLONG)tms.tms_utime * 10000000 / ticks;
                        pti.KernelTime.QuadPart = (ULONGLONG)tms.tms_stime * 10000000 / ticks;
                    }

                    SERVER_START_REQ(get_process_info)
                    {
                        req->handle = wine_server_obj_handle( handle );
                        if ((ret = wine_server_call( req )) == STATUS_SUCCESS)
                        {
                            pti.CreateTime.QuadPart = reply->start_time;
                            pti.ExitTime.QuadPart = reply->end_time;
                        }
                    }
                    SERVER_END_REQ;

                    memcpy(info, &pti, sizeof(KERNEL_USER_TIMES));
                    len = sizeof(KERNEL_USER_TIMES);
                }
                if (size > sizeof(KERNEL_USER_TIMES)) ret = STATUS_INFO_LENGTH_MISMATCH;
            }
            else
            {
                len = sizeof(KERNEL_USER_TIMES);
                ret = STATUS_INFO_LENGTH_MISMATCH;
            }
        }
        break;

    case ProcessDebugPort:
        len = sizeof(DWORD_PTR);
        if (size == len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else
            {
                SERVER_START_REQ(get_process_info)
                {
                    req->handle = wine_server_obj_handle( handle );
                    if ((ret = wine_server_call( req )) == STATUS_SUCCESS)
                    {
                        *(DWORD_PTR *)info = reply->debugger_present ? ~(DWORD_PTR)0 : 0;
                    }
                }
                SERVER_END_REQ;
            }
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case ProcessDebugFlags:
        len = sizeof(DWORD);
        if (size == len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else
            {
                SERVER_START_REQ(get_process_info)
                {
                    req->handle = wine_server_obj_handle( handle );
                    if ((ret = wine_server_call( req )) == STATUS_SUCCESS)
                    {
                        *(DWORD *)info = reply->debug_children;
                    }
                }
                SERVER_END_REQ;
            }
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case ProcessDefaultHardErrorMode:
        len = sizeof(process_error_mode);
        if (size == len) memcpy(info, &process_error_mode, len);
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case ProcessDebugObjectHandle:
        /* "These are not the debuggers you are looking for." *
         * set it to 0 aka "no debugger" to satisfy copy protections */
        len = sizeof(HANDLE);
        if (size == len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else if (!handle) ret = STATUS_INVALID_HANDLE;
            else
            {
                memset(info, 0, size);
                ret = STATUS_PORT_NOT_SET;
            }
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case ProcessHandleCount:
        if (size >= 4)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else if (!handle) ret = STATUS_INVALID_HANDLE;
            else
            {
                memset(info, 0, 4);
                len = 4;
            }
            if (size > 4) ret = STATUS_INFO_LENGTH_MISMATCH;
        }
        else
        {
            len = 4;
            ret = STATUS_INFO_LENGTH_MISMATCH;
        }
        break;

    case ProcessAffinityMask:
        len = sizeof(ULONG_PTR);
        if (size == len)
        {
            const ULONG_PTR system_mask = get_system_affinity_mask();

            SERVER_START_REQ(get_process_info)
            {
                req->handle = wine_server_obj_handle( handle );
                if (!(ret = wine_server_call( req )))
                    *(ULONG_PTR *)info = reply->affinity & system_mask;
            }
            SERVER_END_REQ;
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case ProcessWow64Information:
        len = sizeof(ULONG_PTR);
        if (size != len) ret = STATUS_INFO_LENGTH_MISMATCH;
        else if (!info) ret = STATUS_ACCESS_VIOLATION;
        else if (!handle) ret = STATUS_INVALID_HANDLE;
        else
        {
            ULONG_PTR val = 0;

            if (handle == GetCurrentProcess()) val = is_wow64;
            else if (server_cpus & ((1 << CPU_x86_64) | (1 << CPU_ARM64)))
            {
                SERVER_START_REQ( get_process_info )
                {
                    req->handle = wine_server_obj_handle( handle );
                    if (!(ret = wine_server_call( req )))
                        val = (reply->cpu != CPU_x86_64 && reply->cpu != CPU_ARM64);
                }
                SERVER_END_REQ;
            }
            *(ULONG_PTR *)info = val;
        }
        break;

    case ProcessImageFileName:
        /* FIXME: Should return a device path */
    case ProcessImageFileNameWin32:
        SERVER_START_REQ(get_dll_info)
        {
            UNICODE_STRING *image_file_name_str = info;

            req->handle = wine_server_obj_handle( handle );
            req->base_address = 0; /* main module */
            wine_server_set_reply( req, image_file_name_str ? image_file_name_str + 1 : NULL,
                                   size > sizeof(UNICODE_STRING) ? size - sizeof(UNICODE_STRING) : 0 );
            ret = wine_server_call( req );
            if (ret == STATUS_BUFFER_TOO_SMALL) ret = STATUS_INFO_LENGTH_MISMATCH;

            len = sizeof(UNICODE_STRING) + reply->filename_len;
            if (ret == STATUS_SUCCESS)
            {
                image_file_name_str->MaximumLength = image_file_name_str->Length = reply->filename_len;
                image_file_name_str->Buffer = (PWSTR)(image_file_name_str + 1);
            }
        }
        SERVER_END_REQ;
        break;

    case ProcessExecuteFlags:
        len = sizeof(ULONG);
        if (size == len) *(ULONG *)info = execute_flags;
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case ProcessPriorityClass:
        len = sizeof(PROCESS_PRIORITY_CLASS);
        if (size == len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else
            {
                PROCESS_PRIORITY_CLASS *priority = info;

                SERVER_START_REQ(get_process_info)
                {
                    req->handle = wine_server_obj_handle( handle );
                    if ((ret = wine_server_call( req )) == STATUS_SUCCESS)
                    {
                        priority->PriorityClass = reply->priority;
                        /* FIXME: Not yet supported by the wineserver */
                        priority->Foreground = FALSE;
                    }
                }
                SERVER_END_REQ;
            }
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case ProcessCookie:
        FIXME( "ProcessCookie (%p,%p,0x%08x,%p) stub\n", handle, info, size, ret_len );
        if (handle == NtCurrentProcess())
        {
            len = sizeof(ULONG);
            if (size == len) *(ULONG *)info = 0;
            else ret = STATUS_INFO_LENGTH_MISMATCH;
        }
        else ret = STATUS_INVALID_PARAMETER;
        break;

    case ProcessImageInformation:
        len = sizeof(SECTION_IMAGE_INFORMATION);
        if (size == len)
        {
            if (info)
            {
                pe_image_info_t pe_info;

                SERVER_START_REQ( get_process_info )
                {
                    req->handle = wine_server_obj_handle( handle );
                    wine_server_set_reply( req, &pe_info, sizeof(pe_info) );
                    if ((ret = wine_server_call( req )) == STATUS_SUCCESS)
                        virtual_fill_image_information( &pe_info, info );
                }
                SERVER_END_REQ;
            }
            else ret = STATUS_ACCESS_VIOLATION;
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    default:
        FIXME("(%p,info_class=%d,%p,0x%08x,%p) Unknown information class\n",
              handle, class, info, size, ret_len );
        ret = STATUS_INVALID_INFO_CLASS;
        break;
    }

    if (ret_len) *ret_len = len;
    return ret;
}


/**********************************************************************
 *           NtSetInformationProcess  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetInformationProcess( HANDLE handle, PROCESSINFOCLASS class, void *info, ULONG size )
{
    NTSTATUS ret = STATUS_SUCCESS;

    switch (class)
    {
    case ProcessDefaultHardErrorMode:
        if (size != sizeof(UINT)) return STATUS_INVALID_PARAMETER;
        process_error_mode = *(UINT *)info;
        break;

    case ProcessAffinityMask:
    {
        const ULONG_PTR system_mask = get_system_affinity_mask();

        if (size != sizeof(DWORD_PTR)) return STATUS_INVALID_PARAMETER;
        if (*(PDWORD_PTR)info & ~system_mask)
            return STATUS_INVALID_PARAMETER;
        if (!*(PDWORD_PTR)info)
            return STATUS_INVALID_PARAMETER;
        SERVER_START_REQ( set_process_info )
        {
            req->handle   = wine_server_obj_handle( handle );
            req->affinity = *(PDWORD_PTR)info;
            req->mask     = SET_PROCESS_INFO_AFFINITY;
            ret = wine_server_call( req );
        }
        SERVER_END_REQ;
        break;
    }
    case ProcessPriorityClass:
        if (size != sizeof(PROCESS_PRIORITY_CLASS)) return STATUS_INVALID_PARAMETER;
        else
        {
            PROCESS_PRIORITY_CLASS* ppc = info;

            SERVER_START_REQ( set_process_info )
            {
                req->handle   = wine_server_obj_handle( handle );
                /* FIXME Foreground isn't used */
                req->priority = ppc->PriorityClass;
                req->mask     = SET_PROCESS_INFO_PRIORITY;
                ret = wine_server_call( req );
            }
            SERVER_END_REQ;
        }
        break;

    case ProcessExecuteFlags:
        if (wine_is_64bit() || size != sizeof(ULONG)) return STATUS_INVALID_PARAMETER;
        if (execute_flags & MEM_EXECUTE_OPTION_PERMANENT) return STATUS_ACCESS_DENIED;
        else
        {
            BOOL enable;
            switch (*(ULONG *)info & (MEM_EXECUTE_OPTION_ENABLE|MEM_EXECUTE_OPTION_DISABLE))
            {
            case MEM_EXECUTE_OPTION_ENABLE:
                enable = TRUE;
                break;
            case MEM_EXECUTE_OPTION_DISABLE:
                enable = FALSE;
                break;
            default:
                return STATUS_INVALID_PARAMETER;
            }
            execute_flags = *(ULONG *)info;
            virtual_set_force_exec( enable );
        }
        break;

    case ProcessThreadStackAllocation:
    {
        void *addr = NULL;
        SIZE_T reserve;
        PROCESS_STACK_ALLOCATION_INFORMATION *stack = info;
        if (size == sizeof(PROCESS_STACK_ALLOCATION_INFORMATION_EX))
            stack = &((PROCESS_STACK_ALLOCATION_INFORMATION_EX *)info)->AllocInfo;
        else if (size != sizeof(*stack)) return STATUS_INFO_LENGTH_MISMATCH;

        reserve = stack->ReserveSize;
        ret = NtAllocateVirtualMemory( GetCurrentProcess(), &addr, stack->ZeroBits, &reserve,
                                       MEM_RESERVE, PAGE_READWRITE );
        if (!ret)
        {
#ifdef VALGRIND_STACK_REGISTER
            VALGRIND_STACK_REGISTER( addr, (char *)addr + reserve );
#endif
            stack->StackBase = addr;
        }
        break;
    }

    default:
        FIXME( "(%p,0x%08x,%p,0x%08x) stub\n", handle, class, info, size );
        ret = STATUS_NOT_IMPLEMENTED;
        break;
    }
    return ret;
}


/**********************************************************************
 *           NtOpenProcess  (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenProcess( HANDLE *handle, ACCESS_MASK access,
                               const OBJECT_ATTRIBUTES *attr, const CLIENT_ID *id )
{
    NTSTATUS status;

    SERVER_START_REQ( open_process )
    {
        req->pid        = HandleToULong( id->UniqueProcess );
        req->access     = access;
        req->attributes = attr ? attr->Attributes : 0;
        status = wine_server_call( req );
        if (!status) *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return status;
}


/**********************************************************************
 *           NtSuspendProcess  (NTDLL.@)
 */
NTSTATUS WINAPI NtSuspendProcess( HANDLE handle )
{
    NTSTATUS ret;

    SERVER_START_REQ( suspend_process )
    {
        req->handle = wine_server_obj_handle( handle );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}


/**********************************************************************
 *           NtResumeProcess  (NTDLL.@)
 */
NTSTATUS WINAPI NtResumeProcess( HANDLE handle )
{
    NTSTATUS ret;

    SERVER_START_REQ( resume_process )
    {
        req->handle = wine_server_obj_handle( handle );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}


/**********************************************************************
 *           NtDebugActiveProcess  (NTDLL.@)
 */
NTSTATUS WINAPI NtDebugActiveProcess( HANDLE process, HANDLE debug_object )
{
    FIXME( "(%p %p), stub!\n", process, debug_object );
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           __wine_make_process_system   (NTDLL.@)
 *
 * Mark the current process as a system process.
 * Returns the event that is signaled when all non-system processes have exited.
 */
HANDLE CDECL __wine_make_process_system(void)
{
    HANDLE ret = 0;

    /*  CodeWeavers-specific hack:  We need to exclude ourselves
        from the winewrapper's wait-children process.  So we'll
        close the wait-children pipe if it is defined.  */
    const char * HOSTPTR child_pipe = getenv("WINE_WAIT_CHILD_PIPE");
    if (child_pipe)
    {
        int fd = atoi(child_pipe);
        if (fd) close( fd );
        unsetenv("WINE_WAIT_CHILD_PIPE");
    }

    SERVER_START_REQ( make_process_system )
    {
        if (!wine_server_call( req )) ret = wine_server_ptr_handle( reply->event );
    }
    SERVER_END_REQ;
    return ret;
}
