/*
 * Win32 threads
 *
 * Copyright 1996 Alexandre Julliard
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

#include <assert.h>
#include <stdarg.h>
#include <sys/types.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winternl.h"

#include "kernel_private.h"


/***********************************************************************
 *           FreeLibraryAndExitThread (KERNEL32.@)
 */
void WINAPI FreeLibraryAndExitThread(HINSTANCE hLibModule, DWORD dwExitCode)
{
    FreeLibrary(hLibModule);
    ExitThread(dwExitCode);
}


/***********************************************************************
 * Wow64SetThreadContext [KERNEL32.@]
 */
BOOL WINAPI Wow64SetThreadContext( HANDLE handle, const WOW64_CONTEXT *context)
{
#if defined(__i386__) || defined(__i386_on_x86_64__)
    NTSTATUS status = NtSetContextThread( handle, (const CONTEXT *)context );
#elif defined(__x86_64__)
    NTSTATUS status = RtlWow64SetThreadContext( handle, context );
#else
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;
#endif
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}

/***********************************************************************
 * Wow64GetThreadContext [KERNEL32.@]
 */
BOOL WINAPI Wow64GetThreadContext( HANDLE handle, WOW64_CONTEXT *context)
{
#if defined(__i386__) || defined(__i386_on_x86_64__)
    NTSTATUS status = NtGetContextThread( handle, (CONTEXT *)context );
#elif defined(__x86_64__)
    NTSTATUS status = RtlWow64GetThreadContext( handle, context );
#else
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;
#endif
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}

#ifdef __i386_on_x86_64__
/* We need a direct 32-bit implementation of this in case the caller is requesting
   its own context.  Thunking to 64-bit mode would give us a context that won't
   make any sense to the caller. */
__ASM_THUNK_STDCALL( Wow64GetThreadContext, 8,
                     "jmp " __ASM_THUNK_STDCALL_SYMBOL("GetThreadContext", 8) )
#endif


/**********************************************************************
 *           SetThreadAffinityMask   (KERNEL32.@)
 */
DWORD_PTR WINAPI SetThreadAffinityMask( HANDLE hThread, DWORD_PTR dwThreadAffinityMask )
{
    NTSTATUS                    status;
    THREAD_BASIC_INFORMATION    tbi;

    status = NtQueryInformationThread( hThread, ThreadBasicInformation, 
                                       &tbi, sizeof(tbi), NULL );
    if (status)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return 0;
    }
    status = NtSetInformationThread( hThread, ThreadAffinityMask, 
                                     &dwThreadAffinityMask,
                                     sizeof(dwThreadAffinityMask));
    if (status)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return 0;
    }
    return tbi.AffinityMask;
}


/***********************************************************************
 *           GetThreadSelectorEntry   (KERNEL32.@)
 */
BOOL WINAPI GetThreadSelectorEntry( HANDLE hthread, DWORD sel, LPLDT_ENTRY ldtent )
{
    THREAD_DESCRIPTOR_INFORMATION tdi;
    NTSTATUS status;

    tdi.Selector = sel;
    status = NtQueryInformationThread( hthread, ThreadDescriptorTableEntry, &tdi, sizeof(tdi), NULL);
    if (status)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return FALSE;
    }
    *ldtent = tdi.Entry;
    return TRUE;
}


/***********************************************************************
 * GetCurrentThread [KERNEL32.@]  Gets pseudohandle for current thread
 *
 * RETURNS
 *    Pseudohandle for the current thread
 */
HANDLE WINAPI KERNEL32_GetCurrentThread(void)
{
    return (HANDLE)~(ULONG_PTR)1;
}

/***********************************************************************
 *		GetCurrentProcessId (KERNEL32.@)
 *
 * Get the current process identifier.
 *
 * RETURNS
 *  current process identifier
 */
DWORD WINAPI KERNEL32_GetCurrentProcessId(void)
{
    return HandleToULong(NtCurrentTeb()->ClientId.UniqueProcess);
}

#ifdef __i386_on_x86_64__
__ASM_THUNK_STDCALL( KERNEL32_GetCurrentProcessId, 0, ".byte 0x64\n\tmovl 0x20,%eax\n\tret" )
#endif

/***********************************************************************
 *		GetCurrentThreadId (KERNEL32.@)
 *
 * Get the current thread identifier.
 *
 * RETURNS
 *  current thread identifier
 */
DWORD WINAPI KERNEL32_GetCurrentThreadId(void)
{
    return HandleToULong(NtCurrentTeb()->ClientId.UniqueThread);
}
