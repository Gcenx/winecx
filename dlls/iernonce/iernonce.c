/*
 * Implementation of the iernonce.dll
 *
 * Copyright 2005 Aric Stewart for CodeWeavers
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

#include "windef.h"
#include "wine/debug.h"
#include "winbase.h"
#include "winnt.h"
#include "winuser.h"
#include "winreg.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(iernonce);

typedef struct {
    HKEY key;
    LPWSTR name;
    DWORD position;
} ordered_key;

typedef struct {
    ordered_key* key;
    DWORD   count;
    HKEY    base_key;
} ordered_sections;

typedef struct {
    LPWSTR value;
    LPWSTR name;
    DWORD position;
} ordered_value;

typedef struct {
    ordered_value* value;
    DWORD   count;
    HKEY    base_key;
} ordered_values;


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	TRACE("(0x%p, %d, %p)\n",hinstDLL,fdwReason,lpvReserved);

	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		/* FIXME: Initialisation */
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
		/* FIXME: Cleanup */
	}

	return TRUE;
}

static void init_ordered_sections(ordered_sections *sections)
{
    sections->count = 0;
    sections->key= NULL;
}

static void init_ordered_values(ordered_values *values)
{
    values->count = 0;
    values->value = NULL;
}


static void close_free_ordered_sections(ordered_sections *sections)
{
    INT i;
    for (i = 0; i < sections->count; i++)
    {
        RegCloseKey(sections->key[i].key);
        HeapFree(GetProcessHeap(),0,sections->key[i].name);
    }
    HeapFree(GetProcessHeap(),0,sections->key);
    init_ordered_sections(sections);
}

static void free_ordered_values(ordered_values *values)
{
    INT i;
    for (i = 0; i < values->count; i++)
    {
        HeapFree(GetProcessHeap(),0,values->value[i].value);
        HeapFree(GetProcessHeap(),0,values->value[i].name);
    }
    HeapFree(GetProcessHeap(),0,values->value);
    init_ordered_values(values);
}

static void set_ordered_key(HKEY base_key, ordered_key *newkey, DWORD position,
        LPCWSTR szName)
{
    newkey->position = position;
    RegOpenKeyW(base_key,szName,&(newkey->key));
    newkey->name = HeapAlloc(GetProcessHeap(),0,
            (lstrlenW(szName)+1) * sizeof(WCHAR));
    strcpyW(newkey->name,szName);
}

static void set_ordered_value(HKEY base_key, ordered_value *newkey,
        DWORD position, LPCWSTR szName)
{
    DWORD sz = 0;
    newkey->position = position;
    newkey->name = HeapAlloc(GetProcessHeap(),0,
            (lstrlenW(szName)+1) * sizeof(WCHAR));
    strcpyW(newkey->name,szName);
    RegQueryValueExW(base_key,szName,NULL,NULL,NULL,&sz);
    sz ++;
    newkey->value = HeapAlloc(GetProcessHeap(),0,sz);
    RegQueryValueExW(base_key,szName,NULL,NULL,(LPBYTE)newkey->value,&sz);
}


static void insert_section(ordered_sections *sections, DWORD index, 
        DWORD position, LPCWSTR szName)
{
    int i;
    ordered_key* newkeys = HeapAlloc(GetProcessHeap(),0,
           sizeof(ordered_key)*(sections->count + 1));
    for (i = 0; i < index; i++)
        memcpy(&newkeys[i],&sections->key[i],sizeof(ordered_key));

    set_ordered_key(sections->base_key, &newkeys[index],position,szName);

    for (i = index+1; i < sections->count + 1; i++)
        memcpy(&newkeys[i],&sections->key[i-1],sizeof(ordered_key));

    sections->count ++;
    HeapFree(GetProcessHeap(),0,sections->key);
    sections->key = newkeys;
}

static void insert_value(ordered_values *values, DWORD index, 
        DWORD position, LPCWSTR szName)
{
    int i;
    ordered_value* newvalues= HeapReAlloc(GetProcessHeap(),0, values->value,
           sizeof(ordered_value)*(values->count + 1));

    for (i = index+1; i < values->count + 1; i++)
        memcpy(&newvalues[i],&values->value[i-1],sizeof(ordered_value));

    set_ordered_value(values->base_key, &newvalues[index],position,szName);

    values->count ++;
    values->value= newvalues;
}



static void append_section(ordered_sections *sections, DWORD position, 
        LPCWSTR szName)
{
    ordered_key* newkeys;
    if (sections->count > 0)
        newkeys = HeapReAlloc(GetProcessHeap(),0, sections->key,
           sizeof(ordered_key)*(sections->count + 1));
    else
        newkeys = HeapAlloc(GetProcessHeap(),0,sizeof(ordered_key));

    set_ordered_key(sections->base_key, &newkeys[sections->count],position,
            szName);

    sections->count ++;
    sections->key = newkeys;
}

static void append_value(ordered_values *values, DWORD position, 
        LPCWSTR szName)
{
    ordered_value* newvalues;
    if (values->count > 0)
        newvalues= HeapReAlloc(GetProcessHeap(),0, values->value,
           sizeof(ordered_value)*(values->count + 1));
    else
        newvalues= HeapAlloc(GetProcessHeap(),0,sizeof(ordered_value));

    set_ordered_value(values->base_key, &newvalues[values->count],position,
            szName);

    values->count ++;
    values->value= newvalues;
}


static void build_ordered_sections(HKEY hkey, ordered_sections *sections)
{
    INT index = 0;
    DWORD rc;
    WCHAR szName[10];

    sections->base_key = hkey;

    rc = RegEnumKeyW(hkey,index,szName,10);
    while (rc == ERROR_SUCCESS)
    {
        int i;
        DWORD position = strtolW(szName,NULL,10);
        for (i = 0; i < sections->count; i++)
        {
            if (sections->key[i].position > position)
            {
                insert_section(sections,i,position,szName);
                break;
            }
        }
        if (i == sections->count)
            append_section(sections,position,szName);

        index ++;
        rc = RegEnumKeyW(hkey,index,szName,10);
    }
}

static void build_ordered_values(HKEY hkey, ordered_values *values)
{
    INT index = 0;
    DWORD rc;
    DWORD sz;
    WCHAR szName[10];

    values->base_key = hkey;

    sz = 10;
    rc = RegEnumValueW(hkey,index,szName,&sz,NULL,NULL,NULL,NULL);
    while (rc == ERROR_SUCCESS)
    {
        int i;
        DWORD position;
        if (!szName[0])
        {
            index ++;
            sz = 10;
            rc = RegEnumValueW(hkey,index,szName,&sz,NULL,NULL,NULL,NULL);
        }
        position = strtolW(szName,NULL,10);
        for (i = 0; i < values->count; i++)
        {
            if (values->value[i].position > position)
            {
                insert_value(values,i,position,szName);
                break;
            }
        }
        if (i == values->count)
            append_value(values,position,szName);

        index ++;
        sz = 10;
        rc = RegEnumValueW(hkey,index,szName,&sz,NULL,NULL,NULL,NULL);
    }
}

static BOOL run_process( LPWSTR command)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION info;
    BOOL rc;
    static const WCHAR c_collen[] = {'C',':','\\',0};

    memset(&si,0,sizeof(STARTUPINFOW));
    memset(&info,0,sizeof(PROCESS_INFORMATION));

    rc = CreateProcessW(NULL, command, NULL, NULL, FALSE, 0, NULL, c_collen,
            &si, &info);

    if ( !rc )
        return FALSE;
    WaitForSingleObject(info.hProcess,INFINITE);

    return TRUE;
}

static void process_command(LPCWSTR command)
{
    LPWSTR execution_command = NULL;

    TRACE("Process command %s\n",debugstr_w(command));

    if ((command[0]=='|' && command[1] == '|') || ! strchrW(command,'|'))
    {
        /* run a direct command */
        LPCWSTR ptr = command;
        while(*ptr == '|')
            ptr ++;
        execution_command = HeapAlloc(GetProcessHeap(),0,
               (lstrlenW(ptr)+1)* sizeof(WCHAR));
        strcpyW(execution_command,ptr);
    }
    else
    {
        /* run a function from a dll file */
        LPWSTR buff;

        LPWSTR dll = NULL;
        LPWSTR function = NULL;
        LPWSTR arguments = NULL;
        DWORD command_len;
        static const WCHAR rundll[] = {'r','u','n','d','l','l','3','2','.','e','x','e',' ','\"','%','s','\"',',','%','s',' ','%','s',0};
        static const WCHAR rundll_noArg[] = {'r','u','n','d','l','l','3','2','.','e','x','e',' ','\"','%','s','\"',',','%','s',0};

        command_len = lstrlenW(command);
        command_len += lstrlenW(rundll);

        buff = HeapAlloc(GetProcessHeap(),0,(lstrlenW(command)+1) * sizeof(WCHAR));
        strcpyW(buff,command);
        dll = buff;
        function = strchrW(dll,'|');
        if (function)
        {
            *function = '\0';
            function ++;

            arguments = strchrW(function,'|');
            if (arguments)
            {
                *arguments = '\0';
                arguments ++;
            }
        }

        execution_command = HeapAlloc(GetProcessHeap(),0,
               command_len * sizeof(WCHAR));
        if (arguments)
            sprintfW(execution_command, rundll,dll,function,arguments);
        else
            sprintfW(execution_command, rundll_noArg,dll,function);
        HeapFree(GetProcessHeap(),0,buff);
    }

    TRACE("Execution command is %s\n",debugstr_w(execution_command));
    run_process(execution_command);

    HeapFree(GetProcessHeap(),0,execution_command);
}

static void process_runonceex_section(ordered_key *section)
{
    int i;
    ordered_values values;

    init_ordered_values(&values);
    build_ordered_values(section->key,&values);

    for (i = 0; i < values.count; i++)
    {
        process_command(values.value[i].value);
        RegDeleteValueW(section->key,values.value[i].name);
    }

    free_ordered_values(&values);
}

VOID WINAPI RunOnceExProcess( VOID )
{
    HKEY hkey;
    DWORD rc;
    DWORD index = 0;
    ordered_sections sections;

    static const WCHAR szRunOnceKey[] = {'S','o','f','t','w','a','r','e','\\','M','i','c','r','o','s','o','f','t','\\','W','i','n','d','o','w','s','\\','C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\','R','u','n','O','n','c','e','E','x',0};
    
    FIXME("Really really basic implementation\n");

    rc = RegOpenKeyW(HKEY_LOCAL_MACHINE,szRunOnceKey,&hkey);
    if (rc != ERROR_SUCCESS)
    {
        TRACE("No RunOnceEx key\n");
        return;
    }

    init_ordered_sections(&sections);
    build_ordered_sections(hkey,&sections);
    
    for (index = 0; index < sections.count; index ++)
    {
        LPWSTR title = NULL;
        DWORD sz = 0;
        TRACE("Section %i\n",sections.key[index].position);
        RegQueryValueExW(sections.key[index].key,NULL,NULL,NULL,NULL,&sz);
        if (sz > 0)
        {
            title = HeapAlloc(GetProcessHeap(),0,sz);
            RegQueryValueExW(sections.key[index].key,NULL,NULL,NULL,
                    (LPBYTE)title, &sz);
        }
        if (title)
            TRACE("Processing Section %s\n",debugstr_w(title));

        process_runonceex_section(&sections.key[index]);
        RegDeleteKeyW(sections.base_key,sections.key[index].name);
            
        HeapFree(GetProcessHeap(),0,title);
    }
   
    close_free_ordered_sections(&sections);
    RegCloseKey(hkey);
}
