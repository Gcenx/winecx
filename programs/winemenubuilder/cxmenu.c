/*
 * Invoke the CrossOver menu management scripts.
 *
 * Copyright 2012 Francois Gouget
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

#define COBJMACROS

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <errno.h>
#include "wine/debug.h"
#include "cxmenu.h"


WINE_DEFAULT_DEBUG_CHANNEL(menubuilder);


int cx_mode = 1;
int cx_dump_menus = 0;
FILE *cx_menu_file = NULL;

#ifdef __ANDROID__
int cx_write_to_file = 1;
#else
int cx_write_to_file = 0;
#endif


/*
 * Functions to invoke the CrossOver menu management scripts.
 */

static int cx_wineshelllink(const char* link, int is_desktop, const char* root,
                            const char* path, const char* args,
                            const char* icon_name, const char* description, const char* arch)
{
    const char *argv[20];
    int pos = 0;
    int retcode;

    argv[pos++] = "wineshelllink";
    argv[pos++] = "--utf8";
    argv[pos++] = "--root";
    argv[pos++] = root;
    argv[pos++] = "--link";
    argv[pos++] = link;
    argv[pos++] = "--path";
    argv[pos++] = path;
    argv[pos++] = is_desktop ? "--desktop" : "--menu";
    if (args && strlen(args))
    {
        argv[pos++] = "--args";
        argv[pos++] = args;
    }
    if (icon_name)
    {
        argv[pos++] = "--icon";
        argv[pos++] = icon_name;
    }
    if (description && strlen(description))
    {
        argv[pos++] = "--descr";
        argv[pos++] = description;
    }
    if (arch && strlen(arch))
    {
        argv[pos++] = "--arch";
        argv[pos++] = arch;
    }
    argv[pos] = NULL;

    retcode = _spawnvp(_P_WAIT, argv[0], argv);
    if (retcode!=0)
        WINE_ERR("%s returned %d\n", argv[0], retcode);
    return retcode;
}

static char* cx_escape_string(const char* src)
{
    const char* s;
    char *dst, *d;
    DWORD len;

    len=1;
    for (s=src; *s; s++)
    {
        switch (*s)
        {
        case '\"':
        case '\\':
            len+=2;
            break;
        default:
            len+=1;
        }
    }

    dst=d=HeapAlloc(GetProcessHeap(), 0, len);
    for (s=src; *s; s++)
    {
        switch (*s)
        {
        case '\"':
            *d='\\';
            d++;
            *d='\"';
            d++;
            break;
        case '\\':
            *d='\\';
            d++;
            *d='\\';
            d++;
            break;
        default:
            *d=*s;
            d++;
        }
    }
    *d='\0';

    return dst;
}

static void cx_print_value(const char* name, const char* value)
{
    if (value)
    {
        char* str = cx_escape_string(value);
        fprintf(cx_menu_file, "\"%s\" = \"%s\"\n", name, str);
        HeapFree(GetProcessHeap(), 0, str);
    }
}

static void cx_write_profile_value(const char *fname, const char *section,
                                   const char* key, const char* value)
{
    if (value)
    {
        char* str = cx_escape_string(value);
        char *str2 = HeapAlloc(GetProcessHeap(), 0, strlen(str) + 3);
        strcpy(str2, "\"");
        strcat(str2, str);
        strcat(str2, "\"");

        if (WritePrivateProfileStringA(section, key, str2, fname))
            WINE_TRACE("Failed to dump %s for %s : %d\n", key, section, GetLastError());
        HeapFree(GetProcessHeap(), 0, str);
        HeapFree(GetProcessHeap(), 0, str2);
    }
}


static void cx_dump_menu(const char* link, int is_desktop, const char* root,
                         const char* path, const char* args,
                         const char* icon_name, const char* description, const char* arch)
{
    const char *s = "/menuItems.txt";
    char *fname = malloc(strlen(xdg_data_dir) + strlen(s) + 1 );
    sprintf(fname, "%s%s", xdg_data_dir, s);

    /* -----------------------------------------------------------
    **   The Menu hack in particular is tracked by 
    ** CrossOver Hack 13785.
    ** (the whole system of cx_mode hacks in winemenubuilder
    ** is a diff from winehq which does not appear to be tracked
    ** by a bug in the hacks milestone.)
    ** ----------------------------------------------------------- */
    if (!cx_menu_file)
    {
        if (cx_write_to_file)
        {
            if (!(cx_menu_file = fopen(fname, "a+")))
                WINE_TRACE("Could not open menu file %s : %d (%s)\n", fname,
                           errno, strerror(errno));
        }

        if (!cx_menu_file)
            cx_menu_file = stdout;
    }

    if (cx_menu_file == stdout)
    {
        fprintf(cx_menu_file, "[%s]\n", link);
        cx_print_value("IsMenu", (is_desktop ? "0" : "1"));
        cx_print_value("Root", root);
        cx_print_value("Path", path);
        cx_print_value("Args", args);
        cx_print_value("Icon", icon_name);
        cx_print_value("Description", description);
        cx_print_value("Arch", arch);
        fprintf(cx_menu_file, "\n");
    }
    else
    {
        cx_write_profile_value(fname, link, "IsMenu", (is_desktop ? "0" : "1"));
        cx_write_profile_value(fname, link, "Root", root);
        cx_write_profile_value(fname, link, "Path", path);
        cx_write_profile_value(fname, link, "Args", args);
        cx_write_profile_value(fname, link, "Icon", icon_name);
        cx_write_profile_value(fname, link, "Description", description);
        cx_write_profile_value(fname, link, "Arch", arch);
    }
    free(fname);
}

int cx_process_menu(LPCWSTR linkW, BOOL is_desktop, DWORD root_csidl,
                    LPCWSTR pathW, LPCWSTR argsW,
                    LPCSTR icon_name, LPCSTR description, LPCSTR arch)
{
    WCHAR rootW[MAX_PATH];
    char *link, *root, *path, *args;
    int rc;

    link = wchars_to_utf8_chars(linkW);
    SHGetSpecialFolderPathW(NULL, rootW, root_csidl, FALSE);
    root = wchars_to_utf8_chars(rootW);
    path = pathW ? wchars_to_utf8_chars(pathW) : NULL;
    args = argsW ? wchars_to_utf8_chars(argsW) : NULL;

    WINE_TRACE("link='%s' %s: '%s' path='%s' args='%s' icon='%s' desc='%s' arch='%s'\n",
               link, is_desktop ? "desktop" : "menu", root,
               path, args, icon_name, description, arch);

    if (cx_dump_menus || cx_write_to_file)
    {
        rc = 0;
        cx_dump_menu(link, is_desktop, root, path, args, icon_name, description, arch);
    }
    else
        rc = cx_wineshelllink(link, is_desktop, root, path, args, icon_name, description, arch);

    HeapFree(GetProcessHeap(), 0, link);
    HeapFree(GetProcessHeap(), 0, root);
    HeapFree(GetProcessHeap(), 0, path);
    HeapFree(GetProcessHeap(), 0, args);
    return rc;
}

static IShellLinkW* load_link(const WCHAR* link)
{
    HRESULT r;
    IShellLinkW *sl;
    IPersistFile *pf;

    r = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void**)&sl);
    if (SUCCEEDED(r))
        r = IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void**)&pf);
    if (SUCCEEDED(r))
    {
        r = IPersistFile_Load(pf, link, STGM_READ);
        IPersistFile_Release(pf);
    }

    if (FAILED(r))
    {
        IShellLinkW_Release(sl);
        sl = NULL;
    }

    return sl;
}

BOOL cx_link_is_64_bit(IShellLinkW *sl, int recurse_level)
{
    static const WCHAR lnk[] = {'.','l','n','k',0};
    HRESULT hr;
    WCHAR temp[MAX_PATH];
    WCHAR path[MAX_PATH];
    const WCHAR *ext;
    DWORD type;

    hr = IShellLinkW_GetPath(sl, temp, sizeof(temp)/sizeof(temp[0]), NULL, SLGP_RAWPATH);
    if (hr != S_OK || !temp[0])
        return FALSE;
    ExpandEnvironmentStringsW(temp, path, sizeof(path)/sizeof(path[0]));

    ext = PathFindExtensionW(path);
    if (!lstrcmpiW(ext, lnk) && recurse_level < 5)
    {
        IShellLinkW *sl2 = load_link(path);
        if (sl2)
        {
            BOOL ret = cx_link_is_64_bit(sl2, recurse_level + 1);
            IShellLinkW_Release(sl2);
            return ret;
        }
    }

    if (!GetBinaryTypeW(path, &type))
        return FALSE;

    return (type == SCS_64BIT_BINARY);
}

/*
 * A CrossOver winemenubuilder extension.
 */

static BOOL cx_process_dir(WCHAR* dir)
{
    static const WCHAR wWILD[]={'\\','*',0};
    static const WCHAR wDOT[]={'.',0};
    static const WCHAR wDOTDOT[]={'.','.',0};
    static const WCHAR wLNK[]={'.','l','n','k',0};
    static const WCHAR wURL[]={'.','u','r','l',0};
    HANDLE hFind;
    WIN32_FIND_DATAW item;
    int lendir, len;
    WCHAR* path;
    BOOL rc;

    WINE_TRACE("scanning directory %s\n", wine_dbgstr_w(dir));
    lendir = lstrlenW(dir);
    lstrcatW(dir, wWILD);
    hFind=FindFirstFileW(dir, &item);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        WINE_TRACE("unable to open the '%s' directory\n", wine_dbgstr_w(dir));
        return FALSE;
    }

    rc = TRUE;
    path = HeapAlloc(GetProcessHeap(), 0, (lendir+1+MAX_PATH+2+1)*sizeof(WCHAR));
    lstrcpyW(path, dir);
    path[lendir] = '\\';
    while (1)
    {
        if (lstrcmpW(item.cFileName, wDOT) && lstrcmpW(item.cFileName, wDOTDOT))
        {
            WINE_TRACE("  %s\n", wine_dbgstr_w(item.cFileName));
            len=lstrlenW(item.cFileName);
            if ((item.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                (len >= 5 && lstrcmpiW(item.cFileName+len-4, wLNK) == 0) ||
                (len >= 5 && lstrcmpiW(item.cFileName+len-4, wURL) == 0))
            {
                lstrcpyW(path+lendir+1, item.cFileName);
                if (item.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (!(item.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))  /* skip symlinks */
                    {
                        if (!cx_process_dir(path))
                            rc = FALSE;
                    }
                }
                else if (len >= 5 && lstrcmpiW(item.cFileName+len-4, wURL) == 0)
                {
                    WINE_TRACE("  url %s\n", wine_dbgstr_w(path));

                    if (!Process_URL(path, FALSE))
                        rc = FALSE;
                }
                else
                {
                    WINE_TRACE("  link %s\n", wine_dbgstr_w(path));

                    if (!Process_Link(path, FALSE))
                        rc=FALSE;
                }
            }
        }

        if (!FindNextFileW(hFind, &item))
        {
            if (GetLastError() != ERROR_NO_MORE_FILES)
            {
                WINE_TRACE("got error %d while scanning the '%s' directory\n", GetLastError(), wine_dbgstr_w(dir));
                rc = FALSE;
            }
            FindClose(hFind);
            break;
        }
    }

    HeapFree(GetProcessHeap(), 0, path);
    return rc;
}

BOOL cx_process_all_menus(void)
{
    static const DWORD locations[] = {
        /* CSIDL_STARTUP, Not interested in this one */
        CSIDL_DESKTOPDIRECTORY, CSIDL_STARTMENU,
        /* CSIDL_COMMON_STARTUP, Not interested in this one */
        CSIDL_COMMON_DESKTOPDIRECTORY, CSIDL_COMMON_STARTMENU };
    WCHAR dir[MAX_PATH+2]; /* +2 for cx_process_dir() */
    char* unix_dir;
    struct stat st;
    DWORD i, len;
    BOOL rc;

    rc = TRUE;
    for (i = 0; i < sizeof(locations)/sizeof(locations[0]); i++)
    {
        if (!SHGetSpecialFolderPathW(0, dir, locations[i], FALSE))
        {
            WINE_TRACE("unable to get the path of folder %08x\n", locations[i]);
            /* Some special folders are not defined in some bottles
             * so this is not an error
             */
            continue;
        }

        len = lstrlenW(dir);
        if (len >= MAX_PATH)
        {
            /* We've just trashed memory! Hopefully we are OK */
            WINE_TRACE("Ignoring special folder %08x because its path is too long: %s\n", locations[i], wine_dbgstr_w(dir));
            rc = FALSE;
            continue;
        }

        /* Only scan directories. This is particularly important for Desktop
         * which may be a symbolic link to the native desktop.
         */
        unix_dir = wine_get_unix_file_name(dir);
        if (!unix_dir || lstat(unix_dir, &st) || !S_ISDIR(st.st_mode))
            WINE_TRACE("'%s' is not a directory, skipping it\n", unix_dir);
        else if (!cx_process_dir(dir))
            rc = FALSE;
        if (unix_dir)
            HeapFree(GetProcessHeap(), 0, unix_dir);
    }
    return rc;
}
