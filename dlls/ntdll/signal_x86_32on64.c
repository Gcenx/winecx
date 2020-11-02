/*
 * x86-32on64 signal handling routines
 *
 * Copyright 1999, 2005 Alexandre Julliard
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

#ifdef __i386_on_x86_64__

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_MACHINE_SYSARCH_H
# include <machine/sysarch.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYSCALL_H
# include <syscall.h>
#else
# ifdef HAVE_SYS_SYSCALL_H
#  include <sys/syscall.h>
# endif
#endif
#ifdef HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif
#ifdef HAVE_SYS_UCONTEXT_H
# include <sys/ucontext.h>
#endif
#ifdef __APPLE__
# include <mach/mach.h>
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/library.h"
#include "ntdll_misc.h"
#include "wine/exception.h"
#include "wine/debug.h"

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

WINE_DEFAULT_DEBUG_CHANNEL(seh);
WINE_DECLARE_DEBUG_CHANNEL(relay);

/* not defined for x86, so copy the x86_64 definition */
typedef struct DECLSPEC_ALIGN(16) _M128A
{
    ULONGLONG Low;
    LONGLONG High;
} M128A;

typedef struct
{
    WORD ControlWord;
    WORD StatusWord;
    BYTE TagWord;
    BYTE Reserved1;
    WORD ErrorOpcode;
    DWORD ErrorOffset;
    WORD ErrorSelector;
    WORD Reserved2;
    DWORD DataOffset;
    WORD DataSelector;
    WORD Reserved3;
    DWORD MxCsr;
    DWORD MxCsr_Mask;
    M128A FloatRegisters[8];
    M128A XmmRegisters[16];
    BYTE Reserved4[96];
} XMM_SAVE_AREA32;

typedef struct DECLSPEC_ALIGN(16) _CONTEXT64 {
    DWORD64 P1Home;          /* 000 */
    DWORD64 P2Home;          /* 008 */
    DWORD64 P3Home;          /* 010 */
    DWORD64 P4Home;          /* 018 */
    DWORD64 P5Home;          /* 020 */
    DWORD64 P6Home;          /* 028 */

    /* Control flags */
    DWORD ContextFlags;      /* 030 */
    DWORD MxCsr;             /* 034 */

    /* Segment */
    WORD SegCs;              /* 038 */
    WORD SegDs;              /* 03a */
    WORD SegEs;              /* 03c */
    WORD SegFs;              /* 03e */
    WORD SegGs;              /* 040 */
    WORD SegSs;              /* 042 */
    DWORD EFlags;            /* 044 */

    /* Debug */
    DWORD64 Dr0;             /* 048 */
    DWORD64 Dr1;             /* 050 */
    DWORD64 Dr2;             /* 058 */
    DWORD64 Dr3;             /* 060 */
    DWORD64 Dr6;             /* 068 */
    DWORD64 Dr7;             /* 070 */

    /* Integer */
    DWORD64 Rax;             /* 078 */
    DWORD64 Rcx;             /* 080 */
    DWORD64 Rdx;             /* 088 */
    DWORD64 Rbx;             /* 090 */
    DWORD64 Rsp;             /* 098 */
    DWORD64 Rbp;             /* 0a0 */
    DWORD64 Rsi;             /* 0a8 */
    DWORD64 Rdi;             /* 0b0 */
    DWORD64 R8;              /* 0b8 */
    DWORD64 R9;              /* 0c0 */
    DWORD64 R10;             /* 0c8 */
    DWORD64 R11;             /* 0d0 */
    DWORD64 R12;             /* 0d8 */
    DWORD64 R13;             /* 0e0 */
    DWORD64 R14;             /* 0e8 */
    DWORD64 R15;             /* 0f0 */

    /* Counter */
    DWORD64 Rip;             /* 0f8 */

    /* Floating point */
    union {
        XMM_SAVE_AREA32 FltSave;  /* 100 */
        struct {
            M128A Header[2];      /* 100 */
            M128A Legacy[8];      /* 120 */
            M128A Xmm0;           /* 1a0 */
            M128A Xmm1;           /* 1b0 */
            M128A Xmm2;           /* 1c0 */
            M128A Xmm3;           /* 1d0 */
            M128A Xmm4;           /* 1e0 */
            M128A Xmm5;           /* 1f0 */
            M128A Xmm6;           /* 200 */
            M128A Xmm7;           /* 210 */
            M128A Xmm8;           /* 220 */
            M128A Xmm9;           /* 230 */
            M128A Xmm10;          /* 240 */
            M128A Xmm11;          /* 250 */
            M128A Xmm12;          /* 260 */
            M128A Xmm13;          /* 270 */
            M128A Xmm14;          /* 280 */
            M128A Xmm15;          /* 290 */
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;

    /* Vector */
    M128A VectorRegister[26];     /* 300 */
    DWORD64 VectorControl;        /* 4a0 */

    /* Debug control */
    DWORD64 DebugControl;         /* 4a8 */
    DWORD64 LastBranchToRip;      /* 4b0 */
    DWORD64 LastBranchFromRip;    /* 4b8 */
    DWORD64 LastExceptionToRip;   /* 4c0 */
    DWORD64 LastExceptionFromRip; /* 4c8 */
} CONTEXT64;

/***********************************************************************
 * signal context platform-specific definitions
 */
#ifdef linux

#include <asm/prctl.h>
static inline int arch_prctl( int func, void *ptr ) { return syscall( __NR_arch_prctl, func, ptr ); }

#define RAX_sig(context)     ((context)->uc_mcontext.gregs[REG_RAX])
#define RBX_sig(context)     ((context)->uc_mcontext.gregs[REG_RBX])
#define RCX_sig(context)     ((context)->uc_mcontext.gregs[REG_RCX])
#define RDX_sig(context)     ((context)->uc_mcontext.gregs[REG_RDX])
#define RSI_sig(context)     ((context)->uc_mcontext.gregs[REG_RSI])
#define RDI_sig(context)     ((context)->uc_mcontext.gregs[REG_RDI])
#define RBP_sig(context)     ((context)->uc_mcontext.gregs[REG_RBP])
#define R8_sig(context)      ((context)->uc_mcontext.gregs[REG_R8])
#define R9_sig(context)      ((context)->uc_mcontext.gregs[REG_R9])
#define R10_sig(context)     ((context)->uc_mcontext.gregs[REG_R10])
#define R11_sig(context)     ((context)->uc_mcontext.gregs[REG_R11])
#define R12_sig(context)     ((context)->uc_mcontext.gregs[REG_R12])
#define R13_sig(context)     ((context)->uc_mcontext.gregs[REG_R13])
#define R14_sig(context)     ((context)->uc_mcontext.gregs[REG_R14])
#define R15_sig(context)     ((context)->uc_mcontext.gregs[REG_R15])

#define CS_sig(context)      (*((WORD *)&(context)->uc_mcontext.gregs[REG_CSGSFS] + 0))
#define GS_sig(context)      (*((WORD *)&(context)->uc_mcontext.gregs[REG_CSGSFS] + 1))
#define FS_sig(context)      (*((WORD *)&(context)->uc_mcontext.gregs[REG_CSGSFS] + 2))

#define RSP_sig(context)     ((context)->uc_mcontext.gregs[REG_RSP])
#define RIP_sig(context)     ((context)->uc_mcontext.gregs[REG_RIP])
#define EFL_sig(context)     ((context)->uc_mcontext.gregs[REG_EFL])
#define TRAP_sig(context)    ((context)->uc_mcontext.gregs[REG_TRAPNO])
#define ERROR_sig(context)   ((context)->uc_mcontext.gregs[REG_ERR])

#define FPU_sig(context)     ((XMM_SAVE_AREA32 * HOSTPTR)((context)->uc_mcontext.fpregs))

#elif defined (__APPLE__)

#if defined(MAC_OS_X_VERSION_10_15) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_15
typedef _STRUCT_MCONTEXT64_FULL *wine_mcontext;
#else
typedef struct
{
	x86_thread_state64_t    __ss64;
	__uint64_t			    __ds;
	__uint64_t			    __es;
	__uint64_t			    __ss;
	__uint64_t			    __gsbase;
} wine_thread_state;

typedef struct
{
	x86_exception_state64_t     __es;
	wine_thread_state           __ss;
	x86_float_state64_t         __fs;
} * HOSTPTR wine_mcontext;
#endif

#define RAX_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__rax)
#define RBX_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__rbx)
#define RCX_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__rcx)
#define RDX_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__rdx)
#define RSI_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__rsi)
#define RDI_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__rdi)
#define RBP_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__rbp)
#define R8_sig(context)      (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__r8)
#define R9_sig(context)      (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__r9)
#define R10_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__r10)
#define R11_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__r11)
#define R12_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__r12)
#define R13_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__r13)
#define R14_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__r14)
#define R15_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__r15)

#define CS_sig(context)      (*(WORD* HOSTPTR)&((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__cs)
#define DS_sig(context)      (*(WORD* HOSTPTR)&((wine_mcontext)(context)->uc_mcontext)->__ss.__ds)
#define ES_sig(context)      (*(WORD* HOSTPTR)&((wine_mcontext)(context)->uc_mcontext)->__ss.__es)
#define SS_sig(context)      (*(WORD* HOSTPTR)&((wine_mcontext)(context)->uc_mcontext)->__ss.__ss)
#define FS_sig(context)      (*(WORD* HOSTPTR)&((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__fs)
#define GS_sig(context)      (*(WORD* HOSTPTR)&((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__gs)

#define EFL_sig(context)     (((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__rflags)

#define RIP_sig(context)     (*((unsigned long* HOSTPTR)&((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__rip))
#define RSP_sig(context)     (*((unsigned long* HOSTPTR)&((wine_mcontext)(context)->uc_mcontext)->__ss.__ss64.__rsp))

#define TRAP_sig(context)    (((wine_mcontext)(context)->uc_mcontext)->__es.__trapno)
#define ERROR_sig(context)   (((wine_mcontext)(context)->uc_mcontext)->__es.__err)

#define FPU_sig(context)     ((XMM_SAVE_AREA32 * HOSTPTR)&((wine_mcontext)(context)->uc_mcontext)->__fs.__fpu_fcw)

#else
#error You must define the signal context functions for your platform
#endif

/* stack layout when calling an exception raise function */
struct stack_layout
{
    DWORD             return_address;
    DWORD             return_sel;
    DWORD             padding[2];
    CONTEXT64         context64;
    CONTEXT           context;
    EXCEPTION_RECORD  rec;
    ULONG64           red_zone[16];
};
C_ASSERT( sizeof(struct stack_layout) % 16 == 0 );

typedef int (*wine_signal_handler)(unsigned int sig);

static const size_t teb_size = 4096;  /* we reserve one page for the TEB */
static size_t signal_stack_mask;
static size_t signal_stack_size;

static wine_signal_handler handlers[256];

extern void DECLSPEC_NORETURN __wine_syscall_dispatcher( void );
extern NTSTATUS WINAPI __syscall_NtGetContextThread( HANDLE handle, CONTEXT *context );

/* convert from straight ASCII to Unicode without depending on the current codepage */
static inline void ascii_to_unicode( WCHAR *dst, const char *src, size_t len )
{
    while (len--) *dst++ = (unsigned char)*src++;
}

static void* WINAPI __wine_fakedll_dispatcher( const char *module, ULONG ord )
{
    UNICODE_STRING name;
    NTSTATUS status;
    HMODULE base;
    WCHAR *moduleW;
    void *proc = NULL;
    DWORD len = strlen(module);

    TRACE( "(%s, %u)\n", debugstr_a(module), ord );

    if (!(moduleW = RtlAllocateHeap( GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR) )))
        return NULL;

    ascii_to_unicode( moduleW, module, len );
    moduleW[ len ] = 0;
    RtlInitUnicodeString( &name, moduleW );

    status = LdrGetDllHandle( NULL, 0, &name, &base );
    if (status == STATUS_DLL_NOT_FOUND)
        status = LdrLoadDll( NULL, 0, &name, &base );
    if (status == STATUS_SUCCESS)
        status = LdrAddRefDll( LDR_ADDREF_DLL_PIN, base );
    if (status == STATUS_SUCCESS)
        status = LdrGetProcedureAddress( base, NULL, ord, &proc );

    if (status)
        FIXME( "No procedure address found for %s.#%u, status %x\n", debugstr_a(module), ord, status );

    RtlFreeHeap( GetProcessHeap(), 0, moduleW );
    return proc;
}

enum i386_trap_code
{
    TRAP_x86_UNKNOWN    = -1,  /* Unknown fault (TRAP_sig not defined) */
    TRAP_x86_DIVIDE     = 0,   /* Division by zero exception */
    TRAP_x86_TRCTRAP    = 1,   /* Single-step exception */
    TRAP_x86_NMI        = 2,   /* NMI interrupt */
    TRAP_x86_BPTFLT     = 3,   /* Breakpoint exception */
    TRAP_x86_OFLOW      = 4,   /* Overflow exception */
    TRAP_x86_BOUND      = 5,   /* Bound range exception */
    TRAP_x86_PRIVINFLT  = 6,   /* Invalid opcode exception */
    TRAP_x86_DNA        = 7,   /* Device not available exception */
    TRAP_x86_DOUBLEFLT  = 8,   /* Double fault exception */
    TRAP_x86_FPOPFLT    = 9,   /* Coprocessor segment overrun */
    TRAP_x86_TSSFLT     = 10,  /* Invalid TSS exception */
    TRAP_x86_SEGNPFLT   = 11,  /* Segment not present exception */
    TRAP_x86_STKFLT     = 12,  /* Stack fault */
    TRAP_x86_PROTFLT    = 13,  /* General protection fault */
    TRAP_x86_PAGEFLT    = 14,  /* Page fault */
    TRAP_x86_ARITHTRAP  = 16,  /* Floating point exception */
    TRAP_x86_ALIGNFLT   = 17,  /* Alignment check exception */
    TRAP_x86_MCHK       = 18,  /* Machine check exception */
    TRAP_x86_CACHEFLT   = 19   /* SIMD exception (via SIGFPE) if CPU is SSE capable
                                  otherwise Cache flush exception (via SIGSEV) */
};

struct x86_thread_data
{
    DWORD              fs;            /* 1d4 TEB selector */
    DWORD              gs;            /* 1d8 libc selector; update winebuild if you move this! */
    DWORD              dr0;           /* 1dc debug registers */
    DWORD              dr1;           /* 1e0 */
    DWORD              dr2;           /* 1e4 */
    DWORD              dr3;           /* 1e8 */
    DWORD              dr6;           /* 1ec */
    DWORD              dr7;           /* 1f0 */
    void     * HOSTPTR exit_frame;    /* 1f4 exit frame pointer */
    /* the ntdll_thread_data structure follows here */
};

C_ASSERT( offsetof( TEB, SystemReserved2 ) + offsetof( struct x86_thread_data, gs ) == 0x1d8 );
C_ASSERT( offsetof( TEB, SystemReserved2 ) + offsetof( struct x86_thread_data, exit_frame ) == 0x1f4 );

static inline struct x86_thread_data *x86_thread_data(void)
{
    return (struct x86_thread_data *)NtCurrentTeb()->SystemReserved2;
}

/* Exception record for handling exceptions happening inside exception handlers */
typedef struct
{
    EXCEPTION_REGISTRATION_RECORD frame;
    EXCEPTION_REGISTRATION_RECORD *prevFrame;
} EXC_NESTED_FRAME;

extern DWORD CDECL EXC_CallHandler( EXCEPTION_RECORD *record, EXCEPTION_REGISTRATION_RECORD *frame,
                                    CONTEXT *context, EXCEPTION_REGISTRATION_RECORD **dispatcher,
                                    PEXCEPTION_HANDLER handler, PEXCEPTION_HANDLER nested_handler );

/***********************************************************************
 *           dispatch_signal
 */
static inline int dispatch_signal(unsigned int sig)
{
    if (handlers[sig] == NULL) return 0;
    return handlers[sig](sig);
}


/***********************************************************************
 *           get_trap_code
 *
 * Get the trap code for a signal.
 */
static inline enum i386_trap_code get_trap_code( const ucontext_t *sigcontext )
{
#ifdef TRAP_sig
    return TRAP_sig(sigcontext);
#else
    return TRAP_x86_UNKNOWN;  /* unknown trap code */
#endif
}

/***********************************************************************
 *           get_error_code
 *
 * Get the error code for a signal.
 */
static inline WORD get_error_code( const ucontext_t *sigcontext )
{
#ifdef ERROR_sig
    return ERROR_sig(sigcontext);
#else
    return 0;
#endif
}

/***********************************************************************
 *           get_signal_stack
 *
 * Get the base of the signal stack for the current thread.
 */
static inline void *get_signal_stack(void)
{
    return (char *)NtCurrentTeb() + 4096;
}


/***********************************************************************
 *           get_current_teb
 *
 * Get the current teb based on the stack pointer.
 */
static inline TEB *get_current_teb(void)
{
    unsigned long rsp;
    __asm__("movq %%rsp,%0" : "=g" (rsp) );
    return (TEB *)(DWORD)(rsp & ~signal_stack_mask);
}


/*******************************************************************
 *         is_valid_frame
 */
static inline BOOL is_valid_frame( void *frame )
{
    if ((ULONG_PTR)frame & 3) return FALSE;
    return (frame >= NtCurrentTeb()->Tib.StackLimit &&
            (void **)frame < (void **)NtCurrentTeb()->Tib.StackBase - 1);
}

static inline int sel_is_64bit( unsigned short sel )
{
    return sel != wine_32on64_cs32 && sel != wine_32on64_ds32 &&
           wine_ldt_is_system( sel );
}

/*******************************************************************
 *         raise_handler
 *
 * Handler for exceptions happening inside a handler.
 */
static DWORD CDECL raise_handler( EXCEPTION_RECORD *rec, EXCEPTION_REGISTRATION_RECORD *frame,
                                  CONTEXT *context, EXCEPTION_REGISTRATION_RECORD **dispatcher )
{
    if (rec->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND))
        return ExceptionContinueSearch;
    /* We shouldn't get here so we store faulty frame in dispatcher */
    *dispatcher = ((EXC_NESTED_FRAME*)frame)->prevFrame;
    return ExceptionNestedException;
}


/*******************************************************************
 *         unwind_handler
 *
 * Handler for exceptions happening inside an unwind handler.
 */
static DWORD CDECL unwind_handler( EXCEPTION_RECORD *rec, EXCEPTION_REGISTRATION_RECORD *frame,
                                   CONTEXT *context, EXCEPTION_REGISTRATION_RECORD **dispatcher )
{
    if (!(rec->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND)))
        return ExceptionContinueSearch;
    /* We shouldn't get here so we store faulty frame in dispatcher */
    *dispatcher = ((EXC_NESTED_FRAME*)frame)->prevFrame;
    return ExceptionCollidedUnwind;
}


/**********************************************************************
 *           call_stack_handlers
 *
 * Call the stack handlers chain.
 */
static NTSTATUS call_stack_handlers( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    EXCEPTION_REGISTRATION_RECORD *frame, *dispatch, *nested_frame;
    DWORD res;

    frame = NtCurrentTeb()->Tib.ExceptionList;
    nested_frame = NULL;
    while (frame != (EXCEPTION_REGISTRATION_RECORD*)~0UL)
    {
        /* Check frame address */
        if (!is_valid_frame( frame ))
        {
            rec->ExceptionFlags |= EH_STACK_INVALID;
            break;
        }

        /* Call handler */
        TRACE( "calling handler at %p code=%x flags=%x\n",
               frame->Handler, rec->ExceptionCode, rec->ExceptionFlags );
        res = WINE_CALL_IMPL32(EXC_CallHandler)( rec, frame, context, &dispatch, frame->Handler, raise_handler );
        TRACE( "handler at %p returned %x\n", frame->Handler, res );

        if (frame == nested_frame)
        {
            /* no longer nested */
            nested_frame = NULL;
            rec->ExceptionFlags &= ~EH_NESTED_CALL;
        }

        switch(res)
        {
        case ExceptionContinueExecution:
            if (!(rec->ExceptionFlags & EH_NONCONTINUABLE)) return STATUS_SUCCESS;
            return STATUS_NONCONTINUABLE_EXCEPTION;
        case ExceptionContinueSearch:
            break;
        case ExceptionNestedException:
            if (nested_frame < dispatch) nested_frame = dispatch;
            rec->ExceptionFlags |= EH_NESTED_CALL;
            break;
        default:
            return STATUS_INVALID_DISPOSITION;
        }
        frame = frame->Prev;
    }
    return STATUS_UNHANDLED_EXCEPTION;
}


/***********************************************************************
 *           fake_32bit_exception_code[_wrapper]
 *
 * Some 32-bit code to stand in as the location of an exception that
 * actually occurred in 64-bit code.  It just far returns back to 64-bit
 * to restore the original 64-bit state at the point of the exception.
 */
extern void DECLSPEC_HIDDEN fake_32bit_exception_code(void) asm("fake_32bit_exception_code");
__ASM_GLOBAL_FUNC32( fake_32bit_exception_code_wrapper,
                     ".skip 5, 0x90\n" /* nop */
                     "fake_32bit_exception_code:\n\t"
                     ".skip 5, 0x90\n\t" /* nop */
                     "lretl" );


/*******************************************************************
 *		raise_exception
 *
 * Implementation of NtRaiseException.
 */
static NTSTATUS raise_exception( EXCEPTION_RECORD *rec, CONTEXT *context, BOOL first_chance )
{
    NTSTATUS status;

    if (first_chance)
    {
        DWORD c;

        TRACE( "code=%x flags=%x addr=%p ip=%08x tid=%04x\n",
               rec->ExceptionCode, rec->ExceptionFlags, rec->ExceptionAddress,
               context->Eip, GetCurrentThreadId() );
        for (c = 0; c < rec->NumberParameters; c++)
            TRACE( " info[%d]=%08x\n", c, rec->ExceptionInformation[c] );
        if (rec->ExceptionCode == EXCEPTION_WINE_STUB)
        {
            if (rec->ExceptionInformation[1] >> 16)
                MESSAGE( "wine: Call from %p to unimplemented function %s.%s, aborting\n",
                         rec->ExceptionAddress,
                         (char*)rec->ExceptionInformation[0], (char*)rec->ExceptionInformation[1] );
            else
                MESSAGE( "wine: Call from %p to unimplemented function %s.%d, aborting\n",
                         rec->ExceptionAddress,
                         (char*)rec->ExceptionInformation[0], rec->ExceptionInformation[1] );
        }
        else
        {
            TRACE(" eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x\n",
                  context->Eax, context->Ebx, context->Ecx,
                  context->Edx, context->Esi, context->Edi );
            TRACE(" ebp=%08x esp=%08x cs=%04x ds=%04x es=%04x fs=%04x gs=%04x flags=%08x\n",
                  context->Ebp, context->Esp, context->SegCs, context->SegDs,
                  context->SegEs, context->SegFs, context->SegGs, context->EFlags );

            if (context->Eip == (DWORD)fake_32bit_exception_code)
            {
                CONTEXT64 *context64 = (CONTEXT64*)context->Edi;
                TRACE(" underlying 64-bit state:\n");
                TRACE("   rip=%016lx\n", context64->Rip );
                TRACE("   rax=%016lx rbx=%016lx rcx=%016lx rdx=%016lx\n",
                      context64->Rax, context64->Rbx, context64->Rcx, context64->Rdx );
                TRACE("   rsi=%016lx rdi=%016lx rbp=%016lx rsp=%016lx\n",
                      context64->Rsi, context64->Rdi, context64->Rbp, context64->Rsp );
                TRACE("   r8=%016lx  r9=%016lx r10=%016lx r11=%016lx\n",
                      context64->R8, context64->R9, context64->R10, context64->R11 );
                TRACE("   r12=%016lx r13=%016lx r14=%016lx r15=%016lx\n",
                      context64->R12, context64->R13, context64->R14, context64->R15 );
            }
        }
        status = send_debug_event( rec, TRUE, context );
        if (status == DBG_CONTINUE || status == DBG_EXCEPTION_HANDLED)
            return STATUS_SUCCESS;

        /* fix up instruction pointer in context for EXCEPTION_BREAKPOINT */
        if (rec->ExceptionCode == EXCEPTION_BREAKPOINT) context->Eip--;

        if (call_vectored_handlers( rec, context ) == EXCEPTION_CONTINUE_EXECUTION)
            return STATUS_SUCCESS;

        if ((status = call_stack_handlers( rec, context )) != STATUS_UNHANDLED_EXCEPTION)
            return status;
    }

    /* last chance exception */

    status = send_debug_event( rec, FALSE, context );
    if (status != DBG_CONTINUE)
    {
        if (rec->ExceptionFlags & EH_STACK_INVALID)
            WINE_ERR("Exception frame is not in stack limits => unable to dispatch exception.\n");
        else if (rec->ExceptionCode == STATUS_NONCONTINUABLE_EXCEPTION)
            WINE_ERR("Process attempted to continue execution after noncontinuable exception.\n");
        else
            WINE_ERR("Unhandled exception code %x flags %x addr %p\n",
                     rec->ExceptionCode, rec->ExceptionFlags, rec->ExceptionAddress );
        NtTerminateProcess( NtCurrentProcess(), rec->ExceptionCode );
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           init_handler
 *
 * Handler initialization when the full context is not needed.
 * Return the stack pointer to use for pushing the exception data.
 */
static inline void init_handler( const ucontext_t *sigcontext )
{
    TEB *teb = get_current_teb();
    struct x86_thread_data *thread_data = (struct x86_thread_data *)teb->SystemReserved2;

    wine_set_fs( thread_data->fs );
    /* Don't set %gs.  That would corrupt the 64-bit GS.base and have no other effect. */

    if (RSP_sig(sigcontext) >> 32)
    {
        WINE_ERR( "stack pointer above 4G in thread %04x rip %lx rsp %lx\n",
                  GetCurrentThreadId(), (DWORD64)RIP_sig(sigcontext), (DWORD64)RSP_sig(sigcontext) );
        abort_thread(1);
    }
}


/***********************************************************************
 *           fpux_to_fpu
 *
 * Build a standard FPU context from an extended one.
 */
static void fpux_to_fpu( FLOATING_SAVE_AREA *fpu, const XMM_SAVE_AREA32 * HOSTPTR fpux )
{
    unsigned int i, tag, stack_top;

    fpu->ControlWord   = fpux->ControlWord | 0xffff0000;
    fpu->StatusWord    = fpux->StatusWord | 0xffff0000;
    fpu->ErrorOffset   = fpux->ErrorOffset;
    fpu->ErrorSelector = fpux->ErrorSelector | (fpux->ErrorOpcode << 16);
    fpu->DataOffset    = fpux->DataOffset;
    fpu->DataSelector  = fpux->DataSelector;
    fpu->Cr0NpxState   = fpux->StatusWord | 0xffff0000;

    stack_top = (fpux->StatusWord >> 11) & 7;
    fpu->TagWord = 0xffff0000;
    for (i = 0; i < 8; i++)
    {
        memcpy( &fpu->RegisterArea[10 * i], &fpux->FloatRegisters[i], 10 );
        if (!(fpux->TagWord & (1 << i))) tag = 3;  /* empty */
        else
        {
            const M128A * HOSTPTR reg = &fpux->FloatRegisters[(i - stack_top) & 7];
            if ((reg->High & 0x7fff) == 0x7fff)  /* exponent all ones */
            {
                tag = 2;  /* special */
            }
            else if (!(reg->High & 0x7fff))  /* exponent all zeroes */
            {
                if (reg->Low) tag = 2;  /* special */
                else tag = 1;  /* zero */
            }
            else
            {
                if (reg->Low >> 63) tag = 0;  /* valid */
                else tag = 2;  /* special */
            }
        }
        fpu->TagWord |= tag << (2 * i);
    }
}


/***********************************************************************
 *           save_context64
 *
 * Set the register values from a 64-bit sigcontext to a CONTEXT64.
 */
static void save_context64( CONTEXT64 *context, const ucontext_t *sigcontext )
{
    context->Rax    = RAX_sig(sigcontext);
    context->Rcx    = RCX_sig(sigcontext);
    context->Rdx    = RDX_sig(sigcontext);
    context->Rbx    = RBX_sig(sigcontext);
    context->Rsp    = RSP_sig(sigcontext);
    context->Rbp    = RBP_sig(sigcontext);
    context->Rsi    = RSI_sig(sigcontext);
    context->Rdi    = RDI_sig(sigcontext);
    context->R8     = R8_sig(sigcontext);
    context->R9     = R9_sig(sigcontext);
    context->R10    = R10_sig(sigcontext);
    context->R11    = R11_sig(sigcontext);
    context->R12    = R12_sig(sigcontext);
    context->R13    = R13_sig(sigcontext);
    context->R14    = R14_sig(sigcontext);
    context->R15    = R15_sig(sigcontext);
    context->Rip    = RIP_sig(sigcontext);
    context->SegCs  = CS_sig(sigcontext);
    context->SegFs  = FS_sig(sigcontext);
    context->SegGs  = GS_sig(sigcontext);
    context->EFlags = EFL_sig(sigcontext);
#ifdef DS_sig
    context->SegDs  = DS_sig(sigcontext);
#else
    __asm__("movw %%ds,%0" : "=m" (context->SegDs));
#endif
#ifdef ES_sig
    context->SegEs  = ES_sig(sigcontext);
#else
    __asm__("movw %%es,%0" : "=m" (context->SegEs));
#endif
#ifdef SS_sig
    context->SegSs  = SS_sig(sigcontext);
#else
    __asm__("movw %%ss,%0" : "=m" (context->SegSs));
#endif
    context->Dr0    = x86_thread_data()->dr0;
    context->Dr1    = x86_thread_data()->dr1;
    context->Dr2    = x86_thread_data()->dr2;
    context->Dr3    = x86_thread_data()->dr3;
    context->Dr6    = x86_thread_data()->dr6;
    context->Dr7    = x86_thread_data()->dr7;
    if (FPU_sig(sigcontext))
    {
        context->u.FltSave = *FPU_sig(sigcontext);
        context->MxCsr = context->u.FltSave.MxCsr;
    }
}


/***********************************************************************
 *           save_context
 *
 * Set the register values from a sigcontext.
 */
static void save_context( CONTEXT *context, const ucontext_t *sigcontext )
{
    context->ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS | CONTEXT_DEBUG_REGISTERS;
    context->Eax    = RAX_sig(sigcontext);
    context->Ecx    = RCX_sig(sigcontext);
    context->Edx    = RDX_sig(sigcontext);
    context->Ebx    = RBX_sig(sigcontext);
    context->Esp    = RSP_sig(sigcontext);
    context->Ebp    = RBP_sig(sigcontext);
    context->Esi    = RSI_sig(sigcontext);
    context->Edi    = RDI_sig(sigcontext);
    context->Eip    = RIP_sig(sigcontext);
    context->SegCs  = CS_sig(sigcontext);
    context->SegFs  = FS_sig(sigcontext);
    context->SegGs  = GS_sig(sigcontext);
    context->EFlags = EFL_sig(sigcontext);
#ifdef DS_sig
    context->SegDs  = DS_sig(sigcontext);
#else
    __asm__("movw %%ds,%0" : "=m" (context->SegDs));
#endif
#ifdef ES_sig
    context->SegEs  = ES_sig(sigcontext);
#else
    __asm__("movw %%es,%0" : "=m" (context->SegEs));
#endif
#ifdef SS_sig
    context->SegSs  = SS_sig(sigcontext);
#else
    __asm__("movw %%ss,%0" : "=m" (context->SegSs));
#endif
    context->Dr0    = x86_thread_data()->dr0;
    context->Dr1    = x86_thread_data()->dr1;
    context->Dr2    = x86_thread_data()->dr2;
    context->Dr3    = x86_thread_data()->dr3;
    context->Dr6    = x86_thread_data()->dr6;
    context->Dr7    = x86_thread_data()->dr7;
    if (FPU_sig(sigcontext))
    {
        context->ContextFlags |= CONTEXT_FLOATING_POINT | CONTEXT_EXTENDED_REGISTERS;
        *(XMM_SAVE_AREA32 *)context->ExtendedRegisters = *FPU_sig(sigcontext);
        fpux_to_fpu( &context->FloatSave, FPU_sig(sigcontext) );
    }
}


/***********************************************************************
 *           save_fpu
 *
 * Save the thread FPU context.
 */
static inline void save_fpu( CONTEXT *context )
{
    struct
    {
        DWORD ControlWord;
        DWORD StatusWord;
        DWORD TagWord;
        DWORD ErrorOffset;
        DWORD ErrorSelector;
        DWORD DataOffset;
        DWORD DataSelector;
    }
    float_status;

    context->ContextFlags |= CONTEXT_FLOATING_POINT;
    __asm__ __volatile__( "fnsave %0; fwait" : "=m" (context->FloatSave) );

    /* Reset unmasked exceptions status to avoid firing an exception. */
    memcpy(&float_status, &context->FloatSave, sizeof(float_status));
    float_status.StatusWord &= float_status.ControlWord | 0xffffff80;

    __asm__ __volatile__( "fldenv %0" : : "m" (float_status) );
}


/***********************************************************************
 *           save_fpux
 *
 * Save the thread FPU extended context.
 */
static inline void save_fpux( CONTEXT *context )
{
    /* we have to enforce alignment by hand */
    char buffer[sizeof(XMM_SAVE_AREA32) + 16];
    XMM_SAVE_AREA32 *state = (XMM_SAVE_AREA32 *)(((ULONG_PTR)buffer + 15) & ~15);

    context->ContextFlags |= CONTEXT_EXTENDED_REGISTERS;
    __asm__ __volatile__( "fxsave %0" : "=m" (*state) );
    memcpy( context->ExtendedRegisters, state, sizeof(*state) );
}


/***********************************************************************
 *           restore_fpu
 *
 * Restore the FPU extended context to a sigcontext.
 */
static inline void restore_fpu( const CONTEXT *context )
{
    /* we have to enforce alignment by hand */
    char buffer[sizeof(XMM_SAVE_AREA32) + 16];
    XMM_SAVE_AREA32 *state = (XMM_SAVE_AREA32 *)(((ULONG_PTR)buffer + 15) & ~15);

    memcpy( state, context->ExtendedRegisters, sizeof(*state) );
    /* reset the current interrupt status */
    state->StatusWord &= state->ControlWord | 0xff80;
    __asm__ __volatile__( "fxrstor %0" : : "m" (*state) );
}


/***********************************************************************
 *           restore_context
 *
 * Build a sigcontext from the register values.
 */
static void restore_context( const CONTEXT *context, ucontext_t *sigcontext )
{
    XMM_SAVE_AREA32 * HOSTPTR fpu = FPU_sig(sigcontext);

    /* can't restore a 32-bit context into 64-bit mode */
    if (sel_is_64bit(context->SegCs)) return;

    x86_thread_data()->dr0 = context->Dr0;
    x86_thread_data()->dr1 = context->Dr1;
    x86_thread_data()->dr2 = context->Dr2;
    x86_thread_data()->dr3 = context->Dr3;
    x86_thread_data()->dr6 = context->Dr6;
    x86_thread_data()->dr7 = context->Dr7;
    RAX_sig(sigcontext) = context->Eax;
    RBX_sig(sigcontext) = context->Ebx;
    RCX_sig(sigcontext) = context->Ecx;
    RDX_sig(sigcontext) = context->Edx;
    RSI_sig(sigcontext) = context->Esi;
    RDI_sig(sigcontext) = context->Edi;
    RBP_sig(sigcontext) = context->Ebp;
    EFL_sig(sigcontext) = context->EFlags;
    RIP_sig(sigcontext) = context->Eip;
    RSP_sig(sigcontext) = context->Esp;
    CS_sig(sigcontext)  = context->SegCs;
    DS_sig(sigcontext)  = context->SegDs;
    ES_sig(sigcontext)  = context->SegEs;
    FS_sig(sigcontext)  = context->SegFs;
    GS_sig(sigcontext)  = context->SegGs;
    SS_sig(sigcontext)  = context->SegSs;

    if (fpu)
        memcpy( fpu, context->ExtendedRegisters, sizeof(*fpu) );
    else
        restore_fpu( context );
}


static void save_xsave( XMM_SAVE_AREA32 *xsave )
{
    /* we have to enforce alignment by hand */
    char buffer[sizeof(XMM_SAVE_AREA32) + 16];
    XMM_SAVE_AREA32 *state = (XMM_SAVE_AREA32 *)(((ULONG_PTR)buffer + 15) & ~15);

    __asm__ __volatile__( "fxsave (%0)\n\t"                 /* xsave */
                          "movdqa %%xmm0,0xa0(%0)\n\t"      /* xsave->XmmRegisters[0] */
                          "movdqa %%xmm1,0xb0(%0)\n\t"      /* xsave->XmmRegisters[1] */
                          "movdqa %%xmm2,0xc0(%0)\n\t"      /* xsave->XmmRegisters[2] */
                          "movdqa %%xmm3,0xd0(%0)\n\t"      /* xsave->XmmRegisters[3] */
                          "movdqa %%xmm4,0xe0(%0)\n\t"      /* xsave->XmmRegisters[4] */
                          "movdqa %%xmm5,0xf0(%0)\n\t"      /* xsave->XmmRegisters[5] */
                          "movdqa %%xmm6,0x100(%0)\n\t"     /* xsave->XmmRegisters[6] */
                          "movdqa %%xmm7,0x110(%0)\n\t"     /* xsave->XmmRegisters[7] */
                          "movdqa %%xmm8,0x120(%0)\n\t"     /* xsave->XmmRegisters[8] */
                          "movdqa %%xmm9,0x130(%0)\n\t"     /* xsave->XmmRegisters[9] */
                          "movdqa %%xmm10,0x140(%0)\n\t"    /* xsave->XmmRegisters[10] */
                          "movdqa %%xmm11,0x150(%0)\n\t"    /* xsave->XmmRegisters[11] */
                          "movdqa %%xmm12,0x160(%0)\n\t"    /* xsave->XmmRegisters[12] */
                          "movdqa %%xmm13,0x170(%0)\n\t"    /* xsave->XmmRegisters[13] */
                          "movdqa %%xmm14,0x180(%0)\n\t"    /* xsave->XmmRegisters[14] */
                          "movdqa %%xmm15,0x190(%0)\n\t"    /* xsave->XmmRegisters[15] */
                           : : "r" (state) : "memory" );
    memcpy( xsave, state, sizeof(*state) );
}


/***********************************************************************
 *		RtlCaptureContext (NTDLL.@)
 */
__ASM_STDCALL_FUNC( RtlCaptureContext, 4,
                    "pushq %rax\n\t"
                    __ASM_CFI(".cfi_adjust_cfa_offset 8\n\t")
                    "movl " __ASM_EXTRA_DIST "+12(%rsp),%eax\n\t" /* context */
                    "movl $0x10007,(%eax)\n\t" /* context->ContextFlags */
                    "movw %gs,0x8c(%eax)\n\t"  /* context->SegGs */
                    "movw %fs,0x90(%eax)\n\t"  /* context->SegFs */
                    "movw %es,0x94(%eax)\n\t"  /* context->SegEs */
                    "movw %ds,0x98(%eax)\n\t"  /* context->SegDs */
                    "movl %edi,0x9c(%eax)\n\t" /* context->Edi */
                    "movl %esi,0xa0(%eax)\n\t" /* context->Esi */
                    "movl %ebx,0xa4(%eax)\n\t" /* context->Ebx */
                    "movl %edx,0xa8(%eax)\n\t" /* context->Edx */
                    "movl %ecx,0xac(%eax)\n\t" /* context->Ecx */
                    "movl %ebp,0xb4(%eax)\n\t" /* context->Ebp */
                    "movl 8(%rsp),%edx\n\t"
                    "movl %edx,0xb8(%eax)\n\t" /* context->Eip */
                    "movw %cs,0xbc(%eax)\n\t"  /* context->SegCs */
                    "pushfq\n\t"
                    __ASM_CFI(".cfi_adjust_cfa_offset 8\n\t")
                    "popq %rcx\n\t"
                    __ASM_CFI(".cfi_adjust_cfa_offset -8\n\t")
                    "movl %ecx,0xc0(%eax)\n\t" /* context->EFlags */
                    "leaq 16(%rsp),%rdx\n\t"
                    "movl %edx,0xc4(%eax)\n\t" /* context->Esp */
                    "movw %ss,0xc8(%eax)\n\t"  /* context->SegSs */
                    "popq %rcx\n\t"
                    __ASM_CFI(".cfi_adjust_cfa_offset -8\n\t")
                    "movl %ecx,0xb0(%eax)\n\t" /* context->Eax */
                    "ret" )

__ASM_THUNK_STDCALL( RtlCaptureContext, 4,
                     "pushl %eax\n\t"
                     __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
                     "movl 8(%esp),%eax\n\t"    /* context */
                     "movl $0x10007,(%eax)\n\t" /* context->ContextFlags */
                     "movw %gs,0x8c(%eax)\n\t"  /* context->SegGs */
                     "movw %fs,0x90(%eax)\n\t"  /* context->SegFs */
                     "movw %es,0x94(%eax)\n\t"  /* context->SegEs */
                     "movw %ds,0x98(%eax)\n\t"  /* context->SegDs */
                     "movl %edi,0x9c(%eax)\n\t" /* context->Edi */
                     "movl %esi,0xa0(%eax)\n\t" /* context->Esi */
                     "movl %ebx,0xa4(%eax)\n\t" /* context->Ebx */
                     "movl %edx,0xa8(%eax)\n\t" /* context->Edx */
                     "movl %ecx,0xac(%eax)\n\t" /* context->Ecx */
                     "movl 0(%ebp),%edx\n\t"
                     "movl %edx,0xb4(%eax)\n\t" /* context->Ebp */
                     "movl 4(%ebp),%edx\n\t"
                     "movl %edx,0xb8(%eax)\n\t" /* context->Eip */
                     "movw %cs,0xbc(%eax)\n\t"  /* context->SegCs */
                     "pushfl\n\t"
                     __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
                     "popl 0xc0(%eax)\n\t"      /* context->EFlags */
                     __ASM_CFI(".cfi_adjust_cfa_offset -4\n\t")
                     "leal 8(%ebp),%edx\n\t"
                     "movl %edx,0xc4(%eax)\n\t" /* context->Esp */
                     "movw %ss,0xc8(%eax)\n\t"  /* context->SegSs */
                     "popl 0xb0(%eax)\n\t"      /* context->Eax */
                     __ASM_CFI(".cfi_adjust_cfa_offset -4\n\t")
                     "ret $4" )


/***********************************************************************
 *           set_full_cpu_context64
 *
 * Set the new 64-bit CPU context.
 */
extern void set_full_cpu_context64( const CONTEXT64 *context );

__ASM_GLOBAL_FUNC( set_full_cpu_context64,
                   "subq $40,%rsp\n\t"
                   __ASM_CFI(".cfi_adjust_cfa_offset 40\n\t")
                   "ldmxcsr 0x34(%rdi)\n\t"         /* context->MxCsr */
                   "movw 0x38(%rdi),%ax\n\t"        /* context->SegCs */
                   "movq %rax,8(%rsp)\n\t"
                   "movw 0x42(%rdi),%ax\n\t"        /* context->SegSs */
                   "movq %rax,32(%rsp)\n\t"
                   "movq 0x44(%rdi),%rax\n\t"       /* context->Eflags */
                   "movq %rax,16(%rsp)\n\t"
                   "movq 0x80(%rdi),%rcx\n\t"       /* context->Rcx */
                   "movq 0x88(%rdi),%rdx\n\t"       /* context->Rdx */
                   "movq 0x90(%rdi),%rbx\n\t"       /* context->Rbx */
                   "movq 0x98(%rdi),%rax\n\t"       /* context->Rsp */
                   "movq %rax,24(%rsp)\n\t"
                   "movq 0xa0(%rdi),%rbp\n\t"       /* context->Rbp */
                   "movq 0xa8(%rdi),%rsi\n\t"       /* context->Rsi */
                   "movq 0xb8(%rdi),%r8\n\t"        /* context->R8 */
                   "movq 0xc0(%rdi),%r9\n\t"        /* context->R9 */
                   "movq 0xc8(%rdi),%r10\n\t"       /* context->R10 */
                   "movq 0xd0(%rdi),%r11\n\t"       /* context->R11 */
                   "movq 0xd8(%rdi),%r12\n\t"       /* context->R12 */
                   "movq 0xe0(%rdi),%r13\n\t"       /* context->R13 */
                   "movq 0xe8(%rdi),%r14\n\t"       /* context->R14 */
                   "movq 0xf0(%rdi),%r15\n\t"       /* context->R15 */
                   "movq 0xf8(%rdi),%rax\n\t"       /* context->Rip */
                   "movq %rax,(%rsp)\n\t"
                   "fxrstor 0x100(%rdi)\n\t"        /* context->FtlSave */
                   "movdqa 0x1a0(%rdi),%xmm0\n\t"   /* context->Xmm0 */
                   "movdqa 0x1b0(%rdi),%xmm1\n\t"   /* context->Xmm1 */
                   "movdqa 0x1c0(%rdi),%xmm2\n\t"   /* context->Xmm2 */
                   "movdqa 0x1d0(%rdi),%xmm3\n\t"   /* context->Xmm3 */
                   "movdqa 0x1e0(%rdi),%xmm4\n\t"   /* context->Xmm4 */
                   "movdqa 0x1f0(%rdi),%xmm5\n\t"   /* context->Xmm5 */
                   "movdqa 0x200(%rdi),%xmm6\n\t"   /* context->Xmm6 */
                   "movdqa 0x210(%rdi),%xmm7\n\t"   /* context->Xmm7 */
                   "movdqa 0x220(%rdi),%xmm8\n\t"   /* context->Xmm8 */
                   "movdqa 0x230(%rdi),%xmm9\n\t"   /* context->Xmm9 */
                   "movdqa 0x240(%rdi),%xmm10\n\t"  /* context->Xmm10 */
                   "movdqa 0x250(%rdi),%xmm11\n\t"  /* context->Xmm11 */
                   "movdqa 0x260(%rdi),%xmm12\n\t"  /* context->Xmm12 */
                   "movdqa 0x270(%rdi),%xmm13\n\t"  /* context->Xmm13 */
                   "movdqa 0x280(%rdi),%xmm14\n\t"  /* context->Xmm14 */
                   "movdqa 0x290(%rdi),%xmm15\n\t"  /* context->Xmm15 */
                   "movq 0x78(%rdi),%rax\n\t"       /* context->Rax */
                   "movq 0xb0(%rdi),%rdi\n\t"       /* context->Rdi */
                   "iretq" );


/***********************************************************************
 *           set_full_cpu_context
 *
 * Set the new CPU context.
 */
extern void CDECL set_full_cpu_context( const CONTEXT *context );

__ASM_GLOBAL_FUNC32( __ASM_THUNK_NAME(set_full_cpu_context),
                     "movl 4(%esp),%ecx\n\t"
                     "cmpw $0,0x8c(%ecx)\n\t"   /* SegGs, if not 0 */
                     "je 1f\n\t"
                     "movw 0x8c(%ecx),%gs\n"
                     "1:\n\t"
                     "movw 0x90(%ecx),%fs\n\t"  /* SegFs */
                     "movw 0x94(%ecx),%es\n\t"  /* SegEs */
                     "movl 0x9c(%ecx),%edi\n\t" /* Edi */
                     "movl 0xa0(%ecx),%esi\n\t" /* Esi */
                     "movl 0xa4(%ecx),%ebx\n\t" /* Ebx */
                     "movl 0xb4(%ecx),%ebp\n\t" /* Ebp */
                     "movw %ss,%ax\n\t"
                     "cmpw 0xc8(%ecx),%ax\n\t"  /* SegSs */
                     "jne 1f\n\t"
                     /* As soon as we have switched stacks the context structure could
                     * be invalid (when signal handlers are executed for example). Copy
                     * values on the target stack before changing ESP. */
                     "movl 0xc4(%ecx),%eax\n\t" /* Esp */
                     "leal -4*4(%eax),%eax\n\t"
                     "movl 0xc0(%ecx),%edx\n\t" /* EFlags */
                     "movl %edx,3*4(%eax)\n\t"
                     "movl 0xbc(%ecx),%edx\n\t" /* SegCs */
                     "movl %edx,2*4(%eax)\n\t"
                     "movl 0xb8(%ecx),%edx\n\t" /* Eip */
                     "movl %edx,1*4(%eax)\n\t"
                     "movl 0xb0(%ecx),%edx\n\t" /* Eax */
                     "movl %edx,0*4(%eax)\n\t"
                     "pushl 0x98(%ecx)\n\t"     /* SegDs */
                     "movl 0xa8(%ecx),%edx\n\t" /* Edx */
                     "movl 0xac(%ecx),%ecx\n\t" /* Ecx */
                     "popl %ds\n\t"
                     "movl %eax,%esp\n\t"
                     "popl %eax\n\t"
                     "iret\n"
                     /* Restore the context when the stack segment changes. We can't use
                     * the same code as above because we do not know if the stack segment
                     * is 16 or 32 bit, and 'movl' will throw an exception when we try to
                     * access memory above the limit. */
                     "1:\n\t"
                     "movl 0xa8(%ecx),%edx\n\t" /* Edx */
                     "movl 0xb0(%ecx),%eax\n\t" /* Eax */
                     "movw 0xc8(%ecx),%ss\n\t"  /* SegSs */
                     "movl 0xc4(%ecx),%esp\n\t" /* Esp */
                     "pushl 0xc0(%ecx)\n\t"     /* EFlags */
                     "pushl 0xbc(%ecx)\n\t"     /* SegCs */
                     "pushl 0xb8(%ecx)\n\t"     /* Eip */
                     "pushl 0x98(%ecx)\n\t"     /* SegDs */
                     "movl 0xac(%ecx),%ecx\n\t" /* Ecx */
                     "popl %ds\n\t"
                     "iret" )


/***********************************************************************
 *           set_cpu_context
 *
 * Set the new CPU context. Used by NtSetContextThread.
 */
void DECLSPEC_HIDDEN set_cpu_context( const CONTEXT *context )
{
    DWORD flags = context->ContextFlags & ~CONTEXT_i386;

    if (flags & CONTEXT_DEBUG_REGISTERS)
    {
        x86_thread_data()->dr0 = context->Dr0;
        x86_thread_data()->dr1 = context->Dr1;
        x86_thread_data()->dr2 = context->Dr2;
        x86_thread_data()->dr3 = context->Dr3;
        x86_thread_data()->dr6 = context->Dr6;
        x86_thread_data()->dr7 = context->Dr7;
    }
    if (flags & CONTEXT_FULL)
    {
        if (!(flags & CONTEXT_CONTROL))
            FIXME( "setting partial context (%x) not supported\n", flags );
        else if (flags & CONTEXT_SEGMENTS)
        {
            if (sel_is_64bit(context->SegCs))
                FIXME( "setting 64-bit context not supported\n" );
            else
                WINE_CALL_IMPL32(set_full_cpu_context)( context );
        }
        else
        {
            CONTEXT newcontext = *context;
            newcontext.SegDs = wine_get_ds();
            newcontext.SegEs = wine_get_es();
            newcontext.SegFs = wine_get_fs();
            newcontext.SegGs = wine_get_gs();
            WINE_CALL_IMPL32(set_full_cpu_context)( &newcontext );
        }
    }
}


/***********************************************************************
 *           get_server_context_flags
 *
 * Convert CPU-specific flags to generic server flags
 */
static unsigned int get_server_context_flags( DWORD flags )
{
    unsigned int ret = 0;

    flags &= ~CONTEXT_i386;  /* get rid of CPU id */
    if (flags & CONTEXT_CONTROL) ret |= SERVER_CTX_CONTROL;
    if (flags & CONTEXT_INTEGER) ret |= SERVER_CTX_INTEGER;
    if (flags & CONTEXT_SEGMENTS) ret |= SERVER_CTX_SEGMENTS;
    if (flags & CONTEXT_FLOATING_POINT) ret |= SERVER_CTX_FLOATING_POINT;
    if (flags & CONTEXT_DEBUG_REGISTERS) ret |= SERVER_CTX_DEBUG_REGISTERS;
    if (flags & CONTEXT_EXTENDED_REGISTERS) ret |= SERVER_CTX_EXTENDED_REGISTERS;
    return ret;
}


/***********************************************************************
 *           context_to_server
 *
 * Convert a register context to the server format.
 */
NTSTATUS context_to_server( context_t *to, const CONTEXT *from )
{
    DWORD flags = from->ContextFlags & ~CONTEXT_i386;  /* get rid of CPU id */

    memset( to, 0, sizeof(*to) );
    to->cpu = CPU_x86;

    if (flags & CONTEXT_CONTROL)
    {
        to->flags |= SERVER_CTX_CONTROL;
        to->ctl.i386_regs.ebp    = from->Ebp;
        to->ctl.i386_regs.esp    = from->Esp;
        to->ctl.i386_regs.eip    = from->Eip;
        to->ctl.i386_regs.cs     = from->SegCs;
        to->ctl.i386_regs.ss     = from->SegSs;
        to->ctl.i386_regs.eflags = from->EFlags;
    }
    if (flags & CONTEXT_INTEGER)
    {
        to->flags |= SERVER_CTX_INTEGER;
        to->integer.i386_regs.eax = from->Eax;
        to->integer.i386_regs.ebx = from->Ebx;
        to->integer.i386_regs.ecx = from->Ecx;
        to->integer.i386_regs.edx = from->Edx;
        to->integer.i386_regs.esi = from->Esi;
        to->integer.i386_regs.edi = from->Edi;
    }
    if (flags & CONTEXT_SEGMENTS)
    {
        to->flags |= SERVER_CTX_SEGMENTS;
        to->seg.i386_regs.ds = from->SegDs;
        to->seg.i386_regs.es = from->SegEs;
        to->seg.i386_regs.fs = from->SegFs;
        to->seg.i386_regs.gs = from->SegGs;
    }
    if (flags & CONTEXT_FLOATING_POINT)
    {
        to->flags |= SERVER_CTX_FLOATING_POINT;
        to->fp.i386_regs.ctrl     = from->FloatSave.ControlWord;
        to->fp.i386_regs.status   = from->FloatSave.StatusWord;
        to->fp.i386_regs.tag      = from->FloatSave.TagWord;
        to->fp.i386_regs.err_off  = from->FloatSave.ErrorOffset;
        to->fp.i386_regs.err_sel  = from->FloatSave.ErrorSelector;
        to->fp.i386_regs.data_off = from->FloatSave.DataOffset;
        to->fp.i386_regs.data_sel = from->FloatSave.DataSelector;
        to->fp.i386_regs.cr0npx   = from->FloatSave.Cr0NpxState;
        memcpy( to->fp.i386_regs.regs, from->FloatSave.RegisterArea, sizeof(to->fp.i386_regs.regs) );
    }
    if (flags & CONTEXT_DEBUG_REGISTERS)
    {
        to->flags |= SERVER_CTX_DEBUG_REGISTERS;
        to->debug.i386_regs.dr0 = from->Dr0;
        to->debug.i386_regs.dr1 = from->Dr1;
        to->debug.i386_regs.dr2 = from->Dr2;
        to->debug.i386_regs.dr3 = from->Dr3;
        to->debug.i386_regs.dr6 = from->Dr6;
        to->debug.i386_regs.dr7 = from->Dr7;
    }
    if (flags & CONTEXT_EXTENDED_REGISTERS)
    {
        to->flags |= SERVER_CTX_EXTENDED_REGISTERS;
        memcpy( to->ext.i386_regs, from->ExtendedRegisters, sizeof(to->ext.i386_regs) );
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           context_from_server
 *
 * Convert a register context from the server format.
 */
NTSTATUS context_from_server( CONTEXT *to, const context_t *from )
{
    if (from->cpu != CPU_x86) return STATUS_INVALID_PARAMETER;

    to->ContextFlags = CONTEXT_i386;
    if (from->flags & SERVER_CTX_CONTROL)
    {
        to->ContextFlags |= CONTEXT_CONTROL;
        to->Ebp    = from->ctl.i386_regs.ebp;
        to->Esp    = from->ctl.i386_regs.esp;
        to->Eip    = from->ctl.i386_regs.eip;
        to->SegCs  = from->ctl.i386_regs.cs;
        to->SegSs  = from->ctl.i386_regs.ss;
        to->EFlags = from->ctl.i386_regs.eflags;
    }
    if (from->flags & SERVER_CTX_INTEGER)
    {
        to->ContextFlags |= CONTEXT_INTEGER;
        to->Eax = from->integer.i386_regs.eax;
        to->Ebx = from->integer.i386_regs.ebx;
        to->Ecx = from->integer.i386_regs.ecx;
        to->Edx = from->integer.i386_regs.edx;
        to->Esi = from->integer.i386_regs.esi;
        to->Edi = from->integer.i386_regs.edi;
    }
    if (from->flags & SERVER_CTX_SEGMENTS)
    {
        to->ContextFlags |= CONTEXT_SEGMENTS;
        to->SegDs = from->seg.i386_regs.ds;
        to->SegEs = from->seg.i386_regs.es;
        to->SegFs = from->seg.i386_regs.fs;
        to->SegGs = from->seg.i386_regs.gs;
    }
    if (from->flags & SERVER_CTX_FLOATING_POINT)
    {
        to->ContextFlags |= CONTEXT_FLOATING_POINT;
        to->FloatSave.ControlWord   = from->fp.i386_regs.ctrl;
        to->FloatSave.StatusWord    = from->fp.i386_regs.status;
        to->FloatSave.TagWord       = from->fp.i386_regs.tag;
        to->FloatSave.ErrorOffset   = from->fp.i386_regs.err_off;
        to->FloatSave.ErrorSelector = from->fp.i386_regs.err_sel;
        to->FloatSave.DataOffset    = from->fp.i386_regs.data_off;
        to->FloatSave.DataSelector  = from->fp.i386_regs.data_sel;
        to->FloatSave.Cr0NpxState   = from->fp.i386_regs.cr0npx;
        memcpy( to->FloatSave.RegisterArea, from->fp.i386_regs.regs, sizeof(to->FloatSave.RegisterArea) );
    }
    if (from->flags & SERVER_CTX_DEBUG_REGISTERS)
    {
        to->ContextFlags |= CONTEXT_DEBUG_REGISTERS;
        to->Dr0 = from->debug.i386_regs.dr0;
        to->Dr1 = from->debug.i386_regs.dr1;
        to->Dr2 = from->debug.i386_regs.dr2;
        to->Dr3 = from->debug.i386_regs.dr3;
        to->Dr6 = from->debug.i386_regs.dr6;
        to->Dr7 = from->debug.i386_regs.dr7;
    }
    if (from->flags & SERVER_CTX_EXTENDED_REGISTERS)
    {
        to->ContextFlags |= CONTEXT_EXTENDED_REGISTERS;
        memcpy( to->ExtendedRegisters, from->ext.i386_regs, sizeof(to->ExtendedRegisters) );
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *              NtSetContextThread  (NTDLL.@)
 *              ZwSetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetContextThread( HANDLE handle, const CONTEXT *context )
{
    NTSTATUS ret = STATUS_SUCCESS;
    BOOL self = (handle == GetCurrentThread());

    /* debug registers require a server call */
    if (self && (context->ContextFlags & (CONTEXT_DEBUG_REGISTERS & ~CONTEXT_i386)))
        self = (x86_thread_data()->dr0 == context->Dr0 &&
                x86_thread_data()->dr1 == context->Dr1 &&
                x86_thread_data()->dr2 == context->Dr2 &&
                x86_thread_data()->dr3 == context->Dr3 &&
                x86_thread_data()->dr6 == context->Dr6 &&
                x86_thread_data()->dr7 == context->Dr7);

    if (!self)
    {
        context_t server_context;
        context_to_server( &server_context, context );
        ret = set_thread_context( handle, &server_context, &self );
    }

    if (self && ret == STATUS_SUCCESS) set_cpu_context( context );
    return ret;
}


/***********************************************************************
 *              NtGetContextThread  (NTDLL.@)
 *              ZwGetContextThread  (NTDLL.@)
 *
 * Note: we use a small assembly wrapper to save the necessary registers
 *       in case we are fetching the context of the current thread.
 */
NTSTATUS CDECL DECLSPEC_HIDDEN __regs_NtGetContextThread( HANDLE handle, CONTEXT *context, CONTEXT *captured_context )
{
    NTSTATUS ret;
    DWORD needed_flags = context->ContextFlags & ~CONTEXT_i386;
    BOOL self = (handle == GetCurrentThread());

    /* debug registers require a server call */
    if (needed_flags & CONTEXT_DEBUG_REGISTERS) self = FALSE;

    if (!self)
    {
        context_t server_context;
        unsigned int server_flags = get_server_context_flags( context->ContextFlags );

        if ((ret = get_thread_context( handle, &server_context, server_flags, &self ))) return ret;
        if ((ret = context_from_server( context, &server_context ))) return ret;
        needed_flags &= ~context->ContextFlags;
    }

    if (self)
    {
        XMM_SAVE_AREA32 xsave;

        if (needed_flags & CONTEXT_INTEGER)
        {
            context->Eax = captured_context->Eax;
            context->Ebx = captured_context->Ebx;
            context->Ecx = captured_context->Ecx;
            context->Edx = captured_context->Edx;
            context->Esi = captured_context->Esi;
            context->Edi = captured_context->Edi;
            context->ContextFlags |= CONTEXT_INTEGER;
        }
        if (needed_flags & CONTEXT_CONTROL)
        {
            context->Ebp    = captured_context->Ebp;
            context->Eip    = captured_context->Eip;
            context->Eip    = (DWORD)SYSCALL(NtGetContextThread) + 18;
            context->Esp    = captured_context->Esp;
            context->SegCs  = LOWORD(captured_context->SegCs);
            context->SegSs  = LOWORD(captured_context->SegSs);
            context->EFlags = captured_context->EFlags;
            context->ContextFlags |= CONTEXT_CONTROL;
        }
        if (needed_flags & CONTEXT_SEGMENTS)
        {
            context->SegDs = LOWORD(captured_context->SegDs);
            context->SegEs = LOWORD(captured_context->SegEs);
            context->SegFs = LOWORD(captured_context->SegFs);
            context->SegGs = LOWORD(captured_context->SegGs);
            context->ContextFlags |= CONTEXT_SEGMENTS;
        }
        if (needed_flags & (CONTEXT_FLOATING_POINT | CONTEXT_EXTENDED_REGISTERS))
            save_xsave( &xsave );
        if (needed_flags & CONTEXT_FLOATING_POINT)
        {
            fpux_to_fpu( &context->FloatSave, &xsave );
            context->ContextFlags |= CONTEXT_FLOATING_POINT;
        }
        if (needed_flags & CONTEXT_EXTENDED_REGISTERS)
        {
            memcpy( context->ExtendedRegisters, &xsave, sizeof(xsave) );
            context->ContextFlags |= CONTEXT_EXTENDED_REGISTERS;
        }
        /* FIXME: xstate */
        /* update the cached version of the debug registers */
        if (context->ContextFlags & (CONTEXT_DEBUG_REGISTERS & ~CONTEXT_i386))
        {
            x86_thread_data()->dr0 = context->Dr0;
            x86_thread_data()->dr1 = context->Dr1;
            x86_thread_data()->dr2 = context->Dr2;
            x86_thread_data()->dr3 = context->Dr3;
            x86_thread_data()->dr6 = context->Dr6;
            x86_thread_data()->dr7 = context->Dr7;
        }
    }

    if (context->ContextFlags & (CONTEXT_INTEGER & ~CONTEXT_i386))
        TRACE( "%p: eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x\n", handle,
               context->Eax, context->Ebx, context->Ecx, context->Edx, context->Esi, context->Edi );
    if (context->ContextFlags & (CONTEXT_CONTROL & ~CONTEXT_i386))
        TRACE( "%p: ebp=%08x esp=%08x eip=%08x cs=%04x ss=%04x flags=%08x\n", handle,
               context->Ebp, context->Esp, context->Eip, context->SegCs, context->SegSs, context->EFlags );
    if (context->ContextFlags & (CONTEXT_SEGMENTS & ~CONTEXT_i386))
        TRACE( "%p: ds=%04x es=%04x fs=%04x gs=%04x\n", handle,
               context->SegDs, context->SegEs, context->SegFs, context->SegGs );
    if (context->ContextFlags & (CONTEXT_DEBUG_REGISTERS & ~CONTEXT_i386))
        TRACE( "%p: dr0=%08x dr1=%08x dr2=%08x dr3=%08x dr6=%08x dr7=%08x\n", handle,
               context->Dr0, context->Dr1, context->Dr2, context->Dr3, context->Dr6, context->Dr7 );

    return STATUS_SUCCESS;
}

__ASM_STDCALL_FUNC32( __ASM_THUNK_NAME(NtGetContextThread), 8,
                      "pushl %ebp\n\t"
                      __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
                      __ASM_CFI(".cfi_rel_offset %ebp,0\n\t")
                      "movl %esp,%ebp\n\t"
                      __ASM_CFI(".cfi_def_cfa_register %ebp\n\t")
                      "leal -0x2cc(%esp),%esp\n\t"  /* sizeof(CONTEXT) for local */
                      "pushl %esp\n\t"              /* local context */
                      "call " __ASM_THUNK_STDCALL_SYMBOL("RtlCaptureContext",4) "\n\t"
                      "leal 4(%ebp),%eax\n\t"
                      "movl %eax,0xc4(%esp)\n\t"    /* local context->Esp on entry (not before call) */
                      "call 1f\n"
                      "1:\n\t"
                      "popl 0xb8(%esp)\n\t"         /* local context->Eip near entry (not caller) */
                      "pushl %esp\n\t"              /* local context */
                      "pushl 12(%ebp)\n\t"          /* orig context param */
                      "pushl 8(%ebp)\n\t"           /* handle param */
                      "calll " __ASM_THUNK_SYMBOL("__regs_NtGetContextThread") "\n\t"
                      "leave\n\t"
                      __ASM_CFI(".cfi_def_cfa %esp,4\n\t")
                      __ASM_CFI(".cfi_same_value %ebp\n\t")
                      "ret $8" )


/***********************************************************************
 *           is_privileged_instr
 *
 * Check if the fault location is a privileged instruction.
 * Based on the instruction emulation code in dlls/kernel/instr.c.
 */
static inline DWORD is_privileged_instr( CONTEXT *context )
{
    BYTE instr[16];
    unsigned int i, len, prefix_count = 0;

    if (context->SegCs != wine_32on64_cs32) return 0;
    len = virtual_uninterrupted_read_memory( (BYTE *)context->Eip, instr, sizeof(instr) );

    for (i = 0; i < len; i++) switch (instr[i])
    {
    /* instruction prefixes */
    case 0x2e:  /* %cs: */
    case 0x36:  /* %ss: */
    case 0x3e:  /* %ds: */
    case 0x26:  /* %es: */
    case 0x64:  /* %fs: */
    case 0x65:  /* %gs: */
    case 0x66:  /* opcode size */
    case 0x67:  /* addr size */
    case 0xf0:  /* lock */
    case 0xf2:  /* repne */
    case 0xf3:  /* repe */
        if (++prefix_count >= 15) return EXCEPTION_ILLEGAL_INSTRUCTION;
        continue;

    case 0x0f: /* extended instruction */
        if (i == len - 1) return 0;
        switch(instr[i + 1])
        {
        case 0x20: /* mov crX, reg */
        case 0x21: /* mov drX, reg */
        case 0x22: /* mov reg, crX */
        case 0x23: /* mov reg drX */
            return EXCEPTION_PRIV_INSTRUCTION;
        }
        return 0;
    case 0x6c: /* insb (%dx) */
    case 0x6d: /* insl (%dx) */
    case 0x6e: /* outsb (%dx) */
    case 0x6f: /* outsl (%dx) */
    case 0xcd: /* int $xx */
    case 0xe4: /* inb al,XX */
    case 0xe5: /* in (e)ax,XX */
    case 0xe6: /* outb XX,al */
    case 0xe7: /* out XX,(e)ax */
    case 0xec: /* inb (%dx),%al */
    case 0xed: /* inl (%dx),%eax */
    case 0xee: /* outb %al,(%dx) */
    case 0xef: /* outl %eax,(%dx) */
    case 0xf4: /* hlt */
    case 0xfa: /* cli */
    case 0xfb: /* sti */
        return EXCEPTION_PRIV_INSTRUCTION;
    default:
        return 0;
    }
    return 0;
}


#include "pshpack1.h"
union atl_thunk
{
    struct
    {
        DWORD movl;  /* movl this,4(%esp) */
        DWORD this;
        BYTE  jmp;   /* jmp func */
        int   func;
    } t1;
    struct
    {
        BYTE  movl;  /* movl this,ecx */
        DWORD this;
        BYTE  jmp;   /* jmp func */
        int   func;
    } t2;
    struct
    {
        BYTE  movl1; /* movl this,edx */
        DWORD this;
        BYTE  movl2; /* movl func,ecx */
        DWORD func;
        WORD  jmp;   /* jmp ecx */
    } t3;
    struct
    {
        BYTE  movl1; /* movl this,ecx */
        DWORD this;
        BYTE  movl2; /* movl func,eax */
        DWORD func;
        WORD  jmp;   /* jmp eax */
    } t4;
    struct
    {
        DWORD inst1; /* pop ecx
                      * pop eax
                      * push ecx
                      * jmp 4(%eax) */
        WORD  inst2;
    } t5;
};
#include "poppack.h"

/**********************************************************************
 *		check_atl_thunk
 *
 * Check if code destination is an ATL thunk, and emulate it if so.
 */
static BOOL check_atl_thunk( ucontext_t *sigcontext, struct stack_layout *stack )
{
    const union atl_thunk *thunk = (const union atl_thunk *)stack->rec.ExceptionInformation[1];
    union atl_thunk thunk_copy;
    SIZE_T thunk_len;

    if (stack->context.SegCs != wine_32on64_cs32) return FALSE;
    thunk_len = virtual_uninterrupted_read_memory( thunk, &thunk_copy, sizeof(*thunk) );
    if (!thunk_len) return FALSE;

    if (thunk_len >= sizeof(thunk_copy.t1) && thunk_copy.t1.movl == 0x042444c7 &&
                                              thunk_copy.t1.jmp == 0xe9)
    {
        if (!virtual_uninterrupted_write_memory( (DWORD *)stack->context.Esp + 1,
                                                 &thunk_copy.t1.this, sizeof(DWORD) ))
        {
            RIP_sig(sigcontext) = (DWORD_PTR)(&thunk->t1.func + 1) + thunk_copy.t1.func;
            TRACE( "emulating ATL thunk type 1 at %p, func=%08x arg=%08x\n",
                   thunk, (DWORD)RIP_sig(sigcontext), thunk_copy.t1.this );
            return TRUE;
        }
    }
    else if (thunk_len >= sizeof(thunk_copy.t2) && thunk_copy.t2.movl == 0xb9 &&
                                                   thunk_copy.t2.jmp == 0xe9)
    {
        RCX_sig(sigcontext) = thunk_copy.t2.this;
        RIP_sig(sigcontext) = (DWORD_PTR)(&thunk->t2.func + 1) + thunk_copy.t2.func;
        TRACE( "emulating ATL thunk type 2 at %p, func=%08x ecx=%08x\n",
               thunk, (DWORD)RIP_sig(sigcontext), (DWORD)RCX_sig(sigcontext) );
        return TRUE;
    }
    else if (thunk_len >= sizeof(thunk_copy.t3) && thunk_copy.t3.movl1 == 0xba &&
                                                   thunk_copy.t3.movl2 == 0xb9 &&
                                                   thunk_copy.t3.jmp == 0xe1ff)
    {
        RDX_sig(sigcontext) = thunk_copy.t3.this;
        RCX_sig(sigcontext) = thunk_copy.t3.func;
        RIP_sig(sigcontext) = thunk_copy.t3.func;
        TRACE( "emulating ATL thunk type 3 at %p, func=%08x ecx=%08x edx=%08x\n",
               thunk, (DWORD)RIP_sig(sigcontext), (DWORD)RCX_sig(sigcontext), (DWORD)RDX_sig(sigcontext) );
        return TRUE;
    }
    else if (thunk_len >= sizeof(thunk_copy.t4) && thunk_copy.t4.movl1 == 0xb9 &&
                                                   thunk_copy.t4.movl2 == 0xb8 &&
                                                   thunk_copy.t4.jmp == 0xe0ff)
    {
        RCX_sig(sigcontext) = thunk_copy.t4.this;
        RAX_sig(sigcontext) = thunk_copy.t4.func;
        RIP_sig(sigcontext) = thunk_copy.t4.func;
        TRACE( "emulating ATL thunk type 4 at %p, func=%08x eax=%08x ecx=%08x\n",
               thunk, (DWORD)RIP_sig(sigcontext), (DWORD)RAX_sig(sigcontext), (DWORD)RCX_sig(sigcontext) );
        return TRUE;
    }
    else if (thunk_len >= sizeof(thunk_copy.t5) && thunk_copy.t5.inst1 == 0xff515859 &&
                                                   thunk_copy.t5.inst2 == 0x0460)
    {
        DWORD func, sp[2];
        if (virtual_uninterrupted_read_memory( (DWORD *)stack->context.Esp, sp, sizeof(sp) ) == sizeof(sp) &&
            virtual_uninterrupted_read_memory( (DWORD *)sp[1] + 1, &func, sizeof(DWORD) ) == sizeof(DWORD) &&
            !virtual_uninterrupted_write_memory( (DWORD *)stack->context.Esp + 1, &sp[0], sizeof(sp[0]) ))
        {
            RCX_sig(sigcontext) = sp[0];
            RAX_sig(sigcontext) = sp[1];
            RSP_sig(sigcontext) += sizeof(DWORD);
            RIP_sig(sigcontext) = func;
            TRACE( "emulating ATL thunk type 5 at %p, func=%08x eax=%08x ecx=%08x esp=%08x\n",
                   thunk, (DWORD)RIP_sig(sigcontext), (DWORD)RAX_sig(sigcontext),
                   (DWORD)RCX_sig(sigcontext), (DWORD)RSP_sig(sigcontext) );
            return TRUE;
        }
    }

    return FALSE;
}


/***********************************************************************
 *           setup_exception_record
 *
 * Setup the exception record and context on the thread stack.
 */
static struct stack_layout *setup_exception_record( ucontext_t *sigcontext )
{
    struct stack_layout *stack = (struct stack_layout *)(DWORD)(RSP_sig(sigcontext) & ~15);
    DWORD exception_code = 0;

    /* stack sanity checks */

    if ((char *)stack >= (char *)get_signal_stack() &&
        (char *)stack < (char *)get_signal_stack() + signal_stack_size)
    {
        WINE_ERR( "nested exception on signal stack in thread %04x eip %lx esp %lx stack %p-%p\n",
                  GetCurrentThreadId(), (DWORD64)RIP_sig(sigcontext),
                  (DWORD64)RSP_sig(sigcontext), NtCurrentTeb()->Tib.StackLimit,
                  NtCurrentTeb()->Tib.StackBase );
        abort_thread(1);
    }

    if (stack - 1 > stack || /* check for overflow in subtraction */
        (char *)stack <= (char *)NtCurrentTeb()->DeallocationStack ||
        (char *)stack > (char *)NtCurrentTeb()->Tib.StackBase)
    {
        WARN( "exception outside of stack limits in thread %04x eip %lx esp %lx stack %p-%p\n",
              GetCurrentThreadId(), (DWORD64)RIP_sig(sigcontext),
              (DWORD64)RSP_sig(sigcontext), NtCurrentTeb()->Tib.StackLimit,
              NtCurrentTeb()->Tib.StackBase );
    }
    else if ((char *)(stack - 1) < (char *)NtCurrentTeb()->DeallocationStack + 4096)
    {
        /* stack overflow on last page, unrecoverable */
        UINT diff = (char *)NtCurrentTeb()->DeallocationStack + 4096 - (char *)(stack - 1);
        WINE_ERR( "stack overflow %u bytes in thread %04x eip %lx esp %lx stack %p-%p-%p\n",
                  diff, GetCurrentThreadId(), (DWORD64)RIP_sig(sigcontext),
                  (DWORD64)RSP_sig(sigcontext), NtCurrentTeb()->DeallocationStack,
                  NtCurrentTeb()->Tib.StackLimit, NtCurrentTeb()->Tib.StackBase );
        abort_thread(1);
    }
    else if ((char *)(stack - 1) < (char *)NtCurrentTeb()->Tib.StackLimit)
    {
        /* stack access below stack limit, may be recoverable */
        switch (virtual_handle_stack_fault( stack - 1 ))
        {
        case 0:  /* not handled */
        {
            UINT diff = (char *)NtCurrentTeb()->Tib.StackLimit - (char *)(stack - 1);
            WINE_ERR( "stack overflow %u bytes in thread %04x eip %lx esp %lx stack %p-%p-%p\n",
                      diff, GetCurrentThreadId(), (DWORD64)RIP_sig(sigcontext),
                      (DWORD64)RSP_sig(sigcontext), NtCurrentTeb()->DeallocationStack,
                      NtCurrentTeb()->Tib.StackLimit, NtCurrentTeb()->Tib.StackBase );
            abort_thread(1);
        }
        case -1:  /* overflow */
            exception_code = EXCEPTION_STACK_OVERFLOW;
            break;
        }
    }

    stack--;  /* push the stack_layout structure */
#if defined(VALGRIND_MAKE_MEM_UNDEFINED)
    VALGRIND_MAKE_MEM_UNDEFINED(stack, sizeof(*stack));
#elif defined(VALGRIND_MAKE_WRITABLE)
    VALGRIND_MAKE_WRITABLE(stack, sizeof(*stack));
#endif
    stack->rec.ExceptionRecord  = NULL;
    stack->rec.ExceptionCode    = exception_code;
    stack->rec.ExceptionFlags   = EXCEPTION_CONTINUABLE;
    stack->rec.ExceptionAddress = (LPVOID)(DWORD)RIP_sig(sigcontext);
    stack->rec.NumberParameters = 0;

    save_context( &stack->context, sigcontext );

    if (sel_is_64bit(stack->context.SegCs))
    {
        /* Synthesize a 32-bit frame below the 64-bit one which, if resumed,
           will restore the 64-bit state by far-returning to 64-bit
           set_full_cpu_context64(). */

        /* Set up far return destination as a far call would. */
        stack->return_sel = wine_32on64_cs64;
        stack->return_address = (DWORD)set_full_cpu_context64;

        /* Modify the 32-bit context as though we were in the middle of
           fake_32bit_exception_code_wrapper, about to far return. */
        stack->context.Eip = (DWORD)fake_32bit_exception_code;
        stack->context.SegCs = wine_32on64_cs32;
        stack->context.Esp = (DWORD)stack;
        save_context64( &stack->context64, sigcontext );
        stack->context.Edi = (DWORD)&stack->context64; /* parameter for set_full_cpu_context64 */

        stack->rec.ExceptionAddress = (LPVOID)stack->context.Eip;
    }

    return stack;
}


/***********************************************************************
 *           setup_exception
 *
 * Setup a proper stack frame for the raise function, and modify the
 * sigcontext so that the return from the signal handler will call
 * the raise function.
 */
static struct stack_layout *setup_exception( ucontext_t *sigcontext )
{
    init_handler( sigcontext );
    return setup_exception_record( sigcontext );
}


/**********************************************************************
 *		raise_generic_exception
 *
 * Generic raise function for exceptions that don't need special treatment.
 */
static void raise_generic_exception( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    NTSTATUS status;

    status = NtRaiseException( rec, context, TRUE );
    raise_status( status, rec );
}


/***********************************************************************
 *           setup_raise_exception
 *
 * Change context to setup a call to a raise exception function.
 */
static void setup_raise_exception( ucontext_t *sigcontext, struct stack_layout *stack )
{
    CS_sig(sigcontext)  = wine_32on64_cs64;
    RIP_sig(sigcontext) = (ULONG64)raise_generic_exception;
    RDI_sig(sigcontext) = (ULONG64)&stack->rec;
    RSI_sig(sigcontext) = (ULONG64)&stack->context;
    RSP_sig(sigcontext) = (ULONG64)stack;
    /* clear single-step, direction, and align check flag */
    EFL_sig(sigcontext) &= ~(0x100|0x400|0x40000);
    DS_sig(sigcontext) = wine_32on64_ds32;
    ES_sig(sigcontext) = wine_32on64_ds32;
}


/**********************************************************************
 *		get_fpu_code
 *
 * Get the FPU exception code from the FPU status.
 */
static inline DWORD get_fpu_code( const CONTEXT *context )
{
    DWORD status = context->FloatSave.StatusWord & ~(context->FloatSave.ControlWord & 0x3f);

    if (status & 0x01)  /* IE */
    {
        if (status & 0x40)  /* SF */
            return EXCEPTION_FLT_STACK_CHECK;
        else
            return EXCEPTION_FLT_INVALID_OPERATION;
    }
    if (status & 0x02) return EXCEPTION_FLT_DENORMAL_OPERAND;  /* DE flag */
    if (status & 0x04) return EXCEPTION_FLT_DIVIDE_BY_ZERO;    /* ZE flag */
    if (status & 0x08) return EXCEPTION_FLT_OVERFLOW;          /* OE flag */
    if (status & 0x10) return EXCEPTION_FLT_UNDERFLOW;         /* UE flag */
    if (status & 0x20) return EXCEPTION_FLT_INEXACT_RESULT;    /* PE flag */
    return EXCEPTION_FLT_INVALID_OPERATION;  /* generic error */
}


/***********************************************************************
 *           handle_interrupt
 *
 * Handle an interrupt.
 */
static BOOL handle_interrupt( unsigned int interrupt, ucontext_t *sigcontext, struct stack_layout *stack )
{
    switch(interrupt)
    {
    case 0x2d:
        if (!is_wow64)
        {
            /* On Wow64, the upper DWORD of Rax contains garbage, and the debug
             * service is usually not recognized when called from usermode. */
            switch (stack->context.Eax)
            {
                case 1: /* BREAKPOINT_PRINT */
                case 3: /* BREAKPOINT_LOAD_SYMBOLS */
                case 4: /* BREAKPOINT_UNLOAD_SYMBOLS */
                case 5: /* BREAKPOINT_COMMAND_STRING (>= Win2003) */
                    RIP_sig(sigcontext) += 3;
                    return TRUE;
            }
        }
        stack->context.Eip += 3;
        stack->rec.ExceptionCode = EXCEPTION_BREAKPOINT;
        stack->rec.ExceptionAddress = (void *)stack->context.Eip;
        stack->rec.NumberParameters = is_wow64 ? 1 : 3;
        stack->rec.ExceptionInformation[0] = stack->context.Eax;
        stack->rec.ExceptionInformation[1] = stack->context.Ecx;
        stack->rec.ExceptionInformation[2] = stack->context.Edx;
        setup_raise_exception( sigcontext, stack );
        return TRUE;
    default:
        return FALSE;
    }
}


/**********************************************************************
 *		segv_handler
 *
 * Handler for SIGSEGV and related errors.
 */
static void segv_handler( int signal, siginfo_t * HOSTPTR siginfo, void * HOSTPTR sigcontext )
{
    struct stack_layout *stack;
    ucontext_t *context = sigcontext;
    void *stack_ptr = TRUNCCAST( void *, RSP_sig(context) );

    init_handler( sigcontext );

    /* check for exceptions on the signal stack caused by write watches */
    if (get_trap_code(context) == TRAP_x86_PAGEFLT &&
        (char *)stack_ptr >= (char *)get_signal_stack() &&
        (char *)stack_ptr < (char *)get_signal_stack() + signal_stack_size &&
        !virtual_handle_fault( siginfo->si_addr, (get_error_code(context) >> 1) & 0x09, TRUE ))
    {
        return;
    }

    /* check for page fault inside the thread stack */
    if (get_trap_code(context) == TRAP_x86_PAGEFLT)
    {
        switch (virtual_handle_stack_fault( stack_ptr ))
        {
        case 1:  /* handled */
            return;
        case -1:  /* overflow */
            stack = setup_exception_record( context );
            stack->rec.ExceptionCode = EXCEPTION_STACK_OVERFLOW;
            goto done;
        }
    }

    stack = setup_exception_record( context );
    if (stack->rec.ExceptionCode == EXCEPTION_STACK_OVERFLOW) goto done;

    switch(get_trap_code(context))
    {
    case TRAP_x86_OFLOW:   /* Overflow exception */
        stack->rec.ExceptionCode = EXCEPTION_INT_OVERFLOW;
        break;
    case TRAP_x86_BOUND:   /* Bound range exception */
        stack->rec.ExceptionCode = EXCEPTION_ARRAY_BOUNDS_EXCEEDED;
        break;
    case TRAP_x86_PRIVINFLT:   /* Invalid opcode exception */
        stack->rec.ExceptionCode = EXCEPTION_ILLEGAL_INSTRUCTION;
        break;
    case TRAP_x86_STKFLT:  /* Stack fault */
        stack->rec.ExceptionCode = EXCEPTION_STACK_OVERFLOW;
        break;
    case TRAP_x86_SEGNPFLT:  /* Segment not present exception */
    case TRAP_x86_PROTFLT:   /* General protection fault */
    case TRAP_x86_UNKNOWN:   /* Unknown fault code */
        {
            WORD err = get_error_code(context);
            if (!err && (stack->rec.ExceptionCode = is_privileged_instr( &stack->context ))) break;
            if ((err & 7) == 2 && handle_interrupt( err >> 3, context, stack )) return;
            stack->rec.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
            stack->rec.NumberParameters = 2;
            stack->rec.ExceptionInformation[0] = 0;
            /* if error contains a LDT selector, use that as fault address */
            if ((err & 7) == 4 && !wine_ldt_is_system( err | 7 ))
                stack->rec.ExceptionInformation[1] = err & ~7;
            else
                stack->rec.ExceptionInformation[1] = 0xffffffff;
        }
        break;
    case TRAP_x86_PAGEFLT:  /* Page fault */
        stack->rec.NumberParameters = 2;
        stack->rec.ExceptionInformation[0] = (get_error_code(context) >> 1) & 0x09;
        stack->rec.ExceptionInformation[1] = TRUNCCAST( ULONG_PTR, siginfo->si_addr );
        stack->rec.ExceptionCode = virtual_handle_fault( siginfo->si_addr,
                                                         stack->rec.ExceptionInformation[0], FALSE );
        if (!stack->rec.ExceptionCode) return;
        if (stack->rec.ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
            stack->rec.ExceptionInformation[0] == EXCEPTION_EXECUTE_FAULT)
        {
            ULONG flags;
            NtQueryInformationProcess( GetCurrentProcess(), ProcessExecuteFlags,
                                       &flags, sizeof(flags), NULL );
            if (!(flags & MEM_EXECUTE_OPTION_DISABLE_THUNK_EMULATION) && check_atl_thunk( context, stack ))
                return;

            /* send EXCEPTION_EXECUTE_FAULT only if data execution prevention is enabled */
            if (!(flags & MEM_EXECUTE_OPTION_DISABLE))
                stack->rec.ExceptionInformation[0] = EXCEPTION_READ_FAULT;
        }
        break;
    case TRAP_x86_ALIGNFLT:  /* Alignment check exception */
        /* FIXME: pass through exception handler first? */
        if (stack->context.EFlags & 0x00040000)
        {
            EFL_sig(context) &= ~0x00040000;  /* disable AC flag */
            return;
        }
        stack->rec.ExceptionCode = EXCEPTION_DATATYPE_MISALIGNMENT;
        break;
    default:
        WINE_ERR( "Got unexpected trap %d\n", get_trap_code(context) );
        /* fall through */
    case TRAP_x86_NMI:       /* NMI interrupt */
    case TRAP_x86_DNA:       /* Device not available exception */
    case TRAP_x86_DOUBLEFLT: /* Double fault exception */
    case TRAP_x86_TSSFLT:    /* Invalid TSS exception */
    case TRAP_x86_MCHK:      /* Machine check exception */
    case TRAP_x86_CACHEFLT:  /* Cache flush exception */
        stack->rec.ExceptionCode = EXCEPTION_ILLEGAL_INSTRUCTION;
        break;
    }
done:
    setup_raise_exception( context, stack );
}


/**********************************************************************
 *		trap_handler
 *
 * Handler for SIGTRAP.
 */
static void trap_handler( int signal, siginfo_t * HOSTPTR siginfo, void * HOSTPTR sigcontext )
{
    ucontext_t *context = sigcontext;
    struct stack_layout *stack = setup_exception( context );

    switch(get_trap_code(context))
    {
    case TRAP_x86_TRCTRAP:  /* Single-step exception */
        stack->rec.ExceptionCode = EXCEPTION_SINGLE_STEP;
        /* when single stepping can't tell whether this is a hw bp or a
         * single step interrupt. try to avoid as much overhead as possible
         * and only do a server call if there is any hw bp enabled. */
        if (!(stack->context.EFlags & 0x100) || (stack->context.Dr7 & 0xff))
        {
            /* (possible) hardware breakpoint, fetch the debug registers */
            DWORD saved_flags = stack->context.ContextFlags;
            stack->context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            __regs_NtGetContextThread( GetCurrentThread(), &stack->context, NULL );
            stack->context.ContextFlags |= saved_flags;  /* restore flags */
        }
        stack->context.EFlags &= ~0x100;  /* clear single-step flag */
        break;
    case TRAP_x86_BPTFLT:   /* Breakpoint exception */
        stack->rec.ExceptionAddress = (char *)stack->rec.ExceptionAddress - 1;  /* back up over the int3 instruction */
        /* fall through */
    default:
        stack->rec.ExceptionCode = EXCEPTION_BREAKPOINT;
        stack->rec.NumberParameters = is_wow64 ? 1 : 3;
        stack->rec.ExceptionInformation[0] = 0;
        stack->rec.ExceptionInformation[1] = 0; /* FIXME */
        stack->rec.ExceptionInformation[2] = 0; /* FIXME */
        break;
    }
    setup_raise_exception( context, stack );
}


/**********************************************************************
 *		fpe_handler
 *
 * Handler for SIGFPE.
 */
static void fpe_handler( int signal, siginfo_t * HOSTPTR siginfo, void * HOSTPTR sigcontext )
{
    ucontext_t *context = sigcontext;
    struct stack_layout *stack = setup_exception( context );

    switch(get_trap_code(context))
    {
    case TRAP_x86_DIVIDE:   /* Division by zero exception */
        stack->rec.ExceptionCode = EXCEPTION_INT_DIVIDE_BY_ZERO;
        break;
    case TRAP_x86_FPOPFLT:   /* Coprocessor segment overrun */
        stack->rec.ExceptionCode = EXCEPTION_FLT_INVALID_OPERATION;
        break;
    case TRAP_x86_ARITHTRAP:  /* Floating point exception */
    case TRAP_x86_UNKNOWN:    /* Unknown fault code */
        stack->rec.ExceptionCode = get_fpu_code( &stack->context );
        stack->rec.ExceptionAddress = (LPVOID)stack->context.FloatSave.ErrorOffset;
        break;
    case TRAP_x86_CACHEFLT:  /* SIMD exception */
        /* TODO:
         * Behaviour only tested for divide-by-zero exceptions
         * Check for other SIMD exceptions as well */
        if(siginfo->si_code != FPE_FLTDIV && siginfo->si_code != FPE_FLTINV)
            FIXME("untested SIMD exception: %#x. Might not work correctly\n",
                  siginfo->si_code);

        stack->rec.ExceptionCode = STATUS_FLOAT_MULTIPLE_TRAPS;
        stack->rec.NumberParameters = 1;
        /* no idea what meaning is actually behind this but that's what native does */
        stack->rec.ExceptionInformation[0] = 0;
        break;
    default:
        WINE_ERR( "Got unexpected trap %d\n", get_trap_code(context) );
        stack->rec.ExceptionCode = EXCEPTION_FLT_INVALID_OPERATION;
        break;
    }

    setup_raise_exception( context, stack );
}


/**********************************************************************
 *		int_handler
 *
 * Handler for SIGINT.
 *
 * FIXME: should not be calling external functions on the signal stack.
 */
static void int_handler( int signal, siginfo_t * HOSTPTR siginfo, void * HOSTPTR sigcontext )
{
    init_handler( sigcontext );
    if (!dispatch_signal(SIGINT))
    {
        struct stack_layout *stack = setup_exception( sigcontext );
        stack->rec.ExceptionCode = CONTROL_C_EXIT;
        setup_raise_exception( sigcontext, stack );
    }
}

/**********************************************************************
 *		abrt_handler
 *
 * Handler for SIGABRT.
 */
static void abrt_handler( int signal, siginfo_t * HOSTPTR siginfo, void * HOSTPTR sigcontext )
{
    struct stack_layout *stack = setup_exception( sigcontext );
    stack->rec.ExceptionCode  = EXCEPTION_WINE_ASSERTION;
    stack->rec.ExceptionFlags = EH_NONCONTINUABLE;
    setup_raise_exception( sigcontext, stack );
}


/**********************************************************************
 *		quit_handler
 *
 * Handler for SIGQUIT.
 */
static void quit_handler( int signal, siginfo_t * HOSTPTR siginfo, void * HOSTPTR sigcontext )
{
    init_handler( sigcontext );
    abort_thread(0);
}


/**********************************************************************
 *		usr1_handler
 *
 * Handler for SIGUSR1, used to signal a thread that it got suspended.
 */
static void usr1_handler( int signal, siginfo_t * HOSTPTR siginfo, void * HOSTPTR sigcontext )
{
    CONTEXT context;

    init_handler( sigcontext );
    save_context( &context, sigcontext );
    wait_suspend( &context );
    restore_context( &context, sigcontext );
}


/***********************************************************************
 *           __wine_set_signal_handler   (NTDLL.@)
 */
int CDECL __wine_set_signal_handler(unsigned int sig, wine_signal_handler wsh)
{
    if (sig >= ARRAY_SIZE(handlers)) return -1;
    if (handlers[sig] != NULL) return -2;
    handlers[sig] = wsh;
    return 0;
}


/***********************************************************************
 *           locking for LDT routines
 */
static RTL_CRITICAL_SECTION ldt_section;
static RTL_CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &ldt_section,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": ldt_section") }
};
static RTL_CRITICAL_SECTION ldt_section = { &critsect_debug, -1, 0, 0, 0, 0 };
static sigset_t ldt_sigset;

static void ldt_lock(void)
{
    sigset_t sigset;

    pthread_sigmask( SIG_BLOCK, &server_block_set, &sigset );
    RtlEnterCriticalSection( &ldt_section );
    if (ldt_section.RecursionCount == 1) ldt_sigset = sigset;
}

static void ldt_unlock(void)
{
    if (ldt_section.RecursionCount == 1)
    {
        sigset_t sigset = ldt_sigset;
        RtlLeaveCriticalSection( &ldt_section );
        pthread_sigmask( SIG_SETMASK, &sigset, NULL );
    }
    else RtlLeaveCriticalSection( &ldt_section );
}


/**********************************************************************
 *		signal_alloc_thread
 */
NTSTATUS signal_alloc_thread( TEB **teb )
{
    static size_t sigstack_alignment;
    struct x86_thread_data *thread_data;
    SIZE_T size;
    void *addr = NULL;
    NTSTATUS status;

    if (!sigstack_alignment)
    {
        size_t min_size = teb_size + max( MINSIGSTKSZ, 8192 );
        /* find the first power of two not smaller than min_size */
        sigstack_alignment = 12;
        while ((1u << sigstack_alignment) < min_size) sigstack_alignment++;
        signal_stack_mask = (1 << sigstack_alignment) - 1;
        signal_stack_size = (1 << sigstack_alignment) - teb_size;
    }

    size = signal_stack_mask + 1;
    if (!(status = virtual_alloc_aligned( &addr, 0, &size, MEM_COMMIT | MEM_TOP_DOWN,
                                          PAGE_READWRITE, sigstack_alignment )))
    {
        *teb = addr;
        (*teb)->Tib.Self = &(*teb)->Tib;
        (*teb)->Tib.ExceptionList = (void *)~0UL;
        (*teb)->WOW32Reserved = __wine_syscall_dispatcher;
        (*teb)->Spare3 = __wine_fakedll_dispatcher;
        thread_data = (struct x86_thread_data *)(*teb)->SystemReserved2;
        if (!(thread_data->fs = wine_ldt_alloc_fs()))
        {
            size = 0;
            NtFreeVirtualMemory( NtCurrentProcess(), &addr, &size, MEM_RELEASE );
            status = STATUS_TOO_MANY_THREADS;
        }
    }
    return status;
}


/**********************************************************************
 *		signal_free_thread
 */
void signal_free_thread( TEB *teb )
{
    SIZE_T size = 0;
    struct x86_thread_data *thread_data = (struct x86_thread_data *)teb->SystemReserved2;

    wine_ldt_free_fs( thread_data->fs );
    NtFreeVirtualMemory( NtCurrentProcess(), (void **)&teb, &size, MEM_RELEASE );
}


/**********************************************************************
 *		signal_init_thread
 */
void signal_init_thread( TEB *teb )
{
    const WORD fpu_cw = 0x27f;
    struct x86_thread_data *thread_data = (struct x86_thread_data *)teb->SystemReserved2;
    LDT_ENTRY fs_entry;
    stack_t ss;

    wine_ldt_set_base( &fs_entry, teb );
    wine_ldt_set_limit( &fs_entry, teb_size - 1 );
    wine_ldt_set_flags( &fs_entry, WINE_LDT_FLAGS_DATA|WINE_LDT_FLAGS_32BIT );
    wine_ldt_init_fs( thread_data->fs, &fs_entry );
    thread_data->gs = wine_get_gs();
    __asm__ volatile ( "mov %0, %%ds\n\t"
                       "mov %0, %%es\n\t"
                       : : "r" (wine_32on64_ds32) );

    ss.ss_sp    = (char *)teb + teb_size;
    ss.ss_size  = signal_stack_size;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, NULL) == -1) perror( "sigaltstack" );

    __asm__ volatile ("fninit; fldcw %0" : : "m" (fpu_cw));
}

/**********************************************************************
 *		signal_init_process
 */
void signal_init_process(void)
{
    struct sigaction sig_act;

    sig_act.sa_mask = server_block_set;
    sig_act.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;

    sig_act.sa_sigaction = int_handler;
    if (sigaction( SIGINT, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = fpe_handler;
    if (sigaction( SIGFPE, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = abrt_handler;
    if (sigaction( SIGABRT, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = quit_handler;
    if (sigaction( SIGQUIT, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = usr1_handler;
    if (sigaction( SIGUSR1, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = segv_handler;
    if (sigaction( SIGSEGV, &sig_act, NULL ) == -1) goto error;
    if (sigaction( SIGILL, &sig_act, NULL ) == -1) goto error;
    if (sigaction( SIGBUS, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = trap_handler;
    if (sigaction( SIGTRAP, &sig_act, NULL ) == -1) goto error;

    wine_ldt_init_locking( ldt_lock, ldt_unlock );
    return;

 error:
    perror("sigaction");
    exit(1);
}


/*******************************************************************
 *		RtlUnwind (NTDLL.@)
 */
void WINAPI DECLSPEC_HIDDEN __regs_RtlUnwind( EXCEPTION_REGISTRATION_RECORD* pEndFrame, PVOID targetIp,
                                              PEXCEPTION_RECORD pRecord, PVOID retval, CONTEXT *context )
{
    EXCEPTION_RECORD record;
    EXCEPTION_REGISTRATION_RECORD *frame, *dispatch;
    DWORD res;

    context->Eax = (DWORD)retval;

    /* build an exception record, if we do not have one */
    if (!pRecord)
    {
        record.ExceptionCode    = STATUS_UNWIND;
        record.ExceptionFlags   = 0;
        record.ExceptionRecord  = NULL;
        record.ExceptionAddress = (void *)context->Eip;
        record.NumberParameters = 0;
        pRecord = &record;
    }

    pRecord->ExceptionFlags |= EH_UNWINDING | (pEndFrame ? 0 : EH_EXIT_UNWIND);

    TRACE( "code=%x flags=%x\n", pRecord->ExceptionCode, pRecord->ExceptionFlags );
    TRACE( "eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x\n",
           context->Eax, context->Ebx, context->Ecx, context->Edx, context->Esi, context->Edi );
    TRACE( "ebp=%08x esp=%08x eip=%08x cs=%04x ds=%04x fs=%04x gs=%04x flags=%08x\n",
           context->Ebp, context->Esp, context->Eip, LOWORD(context->SegCs), LOWORD(context->SegDs),
           LOWORD(context->SegFs), LOWORD(context->SegGs), context->EFlags );

    /* get chain of exception frames */
    frame = NtCurrentTeb()->Tib.ExceptionList;
    while ((frame != (EXCEPTION_REGISTRATION_RECORD*)~0UL) && (frame != pEndFrame))
    {
        /* Check frame address */
        if (pEndFrame && (frame > pEndFrame))
            raise_status( STATUS_INVALID_UNWIND_TARGET, pRecord );

        if (!is_valid_frame( frame )) raise_status( STATUS_BAD_STACK, pRecord );

        /* Call handler */
        TRACE( "calling handler at %p code=%x flags=%x\n",
               frame->Handler, pRecord->ExceptionCode, pRecord->ExceptionFlags );
        res = WINE_CALL_IMPL32(EXC_CallHandler)( pRecord, frame, context, &dispatch, frame->Handler, unwind_handler );
        TRACE( "handler at %p returned %x\n", frame->Handler, res );

        switch(res)
        {
        case ExceptionContinueSearch:
            break;
        case ExceptionCollidedUnwind:
            frame = dispatch;
            break;
        default:
            raise_status( STATUS_INVALID_DISPOSITION, pRecord );
            break;
        }
        frame = __wine_pop_frame( frame );
    }

    NtSetContextThread( GetCurrentThread(), context );
}

/*******************************************************************
 *		RtlUnwind (NTDLL.@)
 */
__ASM_STDCALL_FUNC32( __ASM_THUNK_NAME(RtlUnwind), 16,
                    "pushl %ebp\n\t"
                    __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
                    __ASM_CFI(".cfi_rel_offset %ebp,0\n\t")
                    "movl %esp,%ebp\n\t"
                    __ASM_CFI(".cfi_def_cfa_register %ebp\n\t")
                    "leal -(0x2cc+8)(%esp),%esp\n\t" /* sizeof(CONTEXT) + alignment */
                    "pushl %eax\n\t"
                    "leal 4(%esp),%eax\n\t"          /* context */
                    "xchgl %eax,(%esp)\n\t"
                    "call " __ASM_THUNK_STDCALL_SYMBOL("RtlCaptureContext",4) "\n\t"
                    "leal 24(%ebp),%eax\n\t"
                    "movl %eax,0xc4(%esp)\n\t"       /* context->Esp */
                    "pushl %esp\n\t"
                    "pushl 20(%ebp)\n\t"
                    "pushl 16(%ebp)\n\t"
                    "pushl 12(%ebp)\n\t"
                    "pushl 8(%ebp)\n\t"
                    "call " __ASM_THUNK_STDCALL_SYMBOL("__regs_RtlUnwind",20) "\n\t"
                    "leave\n\t"
                    __ASM_CFI(".cfi_def_cfa %esp,4\n\t")
                    __ASM_CFI(".cfi_same_value %ebp\n\t")
                    "ret $16" )  /* actually never returns */

NTSTATUS WINAPI __syscall_NtContinue( CONTEXT *context, BOOLEAN alert );

/*******************************************************************
 *		NtRaiseException (NTDLL.@)
 */
NTSTATUS WINAPI NtRaiseException( EXCEPTION_RECORD *rec, CONTEXT *context, BOOL first_chance )
{
    NTSTATUS status = raise_exception( rec, context, first_chance );
    if (status == STATUS_SUCCESS) SYSCALL(NtContinue)(context, FALSE);
    return status;
}


/*******************************************************************
 *		raise_exception_full_context
 *
 * Raise an exception with the full CPU context.
 */
void CDECL DECLSPEC_HIDDEN raise_exception_full_context( EXCEPTION_RECORD *rec, CONTEXT *context, BOOL first_chance )
{
    save_fpu( context );
    save_fpux( context );
    /* FIXME: xstate */
    context->Dr0 = x86_thread_data()->dr0;
    context->Dr1 = x86_thread_data()->dr1;
    context->Dr2 = x86_thread_data()->dr2;
    context->Dr3 = x86_thread_data()->dr3;
    context->Dr6 = x86_thread_data()->dr6;
    context->Dr7 = x86_thread_data()->dr7;
    context->ContextFlags |= CONTEXT_DEBUG_REGISTERS;

    RtlRaiseStatus( NtRaiseException( rec, context, first_chance ));
}


/***********************************************************************
 *		RtlRaiseException (NTDLL.@)
 */
void WINAPI RtlRaiseException( EXCEPTION_RECORD *rec )
{
    WINE_CALL_IMPL32(RtlRaiseException)( rec );
}

__ASM_STDCALL_FUNC32( __ASM_THUNK_NAME(RtlRaiseException), 4,
                     "pushl %ebp\n\t"
                     __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
                     __ASM_CFI(".cfi_rel_offset %ebp,0\n\t")
                     "movl %esp,%ebp\n\t"
                     __ASM_CFI(".cfi_def_cfa_register %ebp\n\t")
                     "leal -0x2cc(%esp),%esp\n\t"  /* sizeof(CONTEXT) */
                     "pushl %esp\n\t"              /* context */
                     "call " __ASM_THUNK_STDCALL_SYMBOL("RtlCaptureContext",4) "\n\t"
                     "movl 4(%ebp),%eax\n\t"       /* return address */
                     "movl 8(%ebp),%ecx\n\t"       /* rec */
                     "movl %eax,12(%ecx)\n\t"      /* rec->ExceptionAddress */
                     "leal 12(%ebp),%eax\n\t"
                     "movl %eax,0xc4(%esp)\n\t"    /* context->Esp */
                     "movl %esp,%eax\n\t"
                     "pushl $1\n\t"
                     "pushl %eax\n\t"
                     "pushl %ecx\n\t"
                     "call " __ASM_THUNK_SYMBOL("raise_exception_full_context") "\n\t"
                     "leave\n\t"
                     __ASM_CFI(".cfi_def_cfa %esp,4\n\t")
                     __ASM_CFI(".cfi_same_value %ebp\n\t")
                     "ret $4" )  /* actually never returns */


/*************************************************************************
 *		RtlCaptureStackBackTrace (NTDLL.@)
 */
USHORT WINAPI RtlCaptureStackBackTrace( ULONG skip, ULONG count, PVOID *buffer, ULONG *hash )
{
    FIXME( "(%d, %d, %p, %p) stub!\n", skip, count, buffer, hash );
    return 0;
}


/* wrapper for apps that don't declare the thread function correctly */
extern DWORD CDECL call_entry( LPTHREAD_START_ROUTINE entry, void *arg );
__ASM_GLOBAL_FUNC32( __ASM_THUNK_NAME(call_entry),
                     "pushl %ebp\n\t"
                     __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
                     __ASM_CFI(".cfi_rel_offset %ebp,0\n\t")
                     "movl %esp,%ebp\n\t"
                     __ASM_CFI(".cfi_def_cfa_register %ebp\n\t")
                     "subl $4,%esp\n\t"
                     "pushl 12(%ebp)\n\t"
                     "call *8(%ebp)\n\t"
                     "leave\n\t"
                     __ASM_CFI(".cfi_def_cfa %esp,4\n\t")
                     __ASM_CFI(".cfi_same_value %ebp\n\t")
                     "ret" )

/***********************************************************************
 *           call_thread_func
 */
extern void WINAPI call_thread_func( LPTHREAD_START_ROUTINE entry, void *arg )
{
    __TRY
    {
        DWORD ret;
        TRACE_(relay)( "\1Starting thread proc %p (arg=%p)\n", entry, arg );
        if (wine_is_thunk32to64(entry))
            ret = entry(arg);
        else
            ret = WINE_CALL_IMPL32(call_entry)( entry, arg );
        RtlExitUserThread( ret );
    }
    __EXCEPT(call_unhandled_exception_filter)
    {
        NtTerminateThread( GetCurrentThread(), GetExceptionCode() );
    }
    __ENDTRY
    abort();  /* should not be reached */
}

extern void CDECL call_thread_func_wrapper(void) DECLSPEC_HIDDEN;
__ASM_GLOBAL_FUNC32( __ASM_THUNK_NAME(call_thread_func_wrapper),
                     "pushl %ebp\n\t"
                     __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
                     __ASM_CFI(".cfi_rel_offset %ebp,0\n\t")
                     "movl %esp,%ebp\n\t"
                     __ASM_CFI(".cfi_def_cfa_register %ebp\n\t")
                     "pushl %ebx\n\t"  /* arg */
                     "pushl %eax\n\t"  /* entry */
                     "call " __ASM_THUNK_SYMBOL("call_thread_func") )


extern void DECLSPEC_NORETURN start_thread( LPTHREAD_START_ROUTINE entry, void *arg, BOOL suspend,
                                            void *relay );
__ASM_GLOBAL_FUNC( start_thread,
                   "subq $56,%rsp\n\t"
                   __ASM_CFI(".cfi_adjust_cfa_offset 56\n\t")
                   "movq %rbp,48(%rsp)\n\t"
                   __ASM_CFI(".cfi_rel_offset %rbp,48\n\t")
                   "movq %rbx,40(%rsp)\n\t"
                   __ASM_CFI(".cfi_rel_offset %rbx,40\n\t")
                   "movq %r12,32(%rsp)\n\t"
                   __ASM_CFI(".cfi_rel_offset %r12,32\n\t")
                   "movq %r13,24(%rsp)\n\t"
                   __ASM_CFI(".cfi_rel_offset %r13,24\n\t")
                   "movq %r14,16(%rsp)\n\t"
                   __ASM_CFI(".cfi_rel_offset %r14,16\n\t")
                   "movq %r15,8(%rsp)\n\t"
                   __ASM_CFI(".cfi_rel_offset %r15,8\n\t")
                   /* store exit frame */
                   "movq %rbp,%fs:0x1f4\n\t"      /* x86_thread_data()->exit_frame */
                   /* switch to thread stack */
                   "movl %fs:4,%eax\n\t"          /* NtCurrentTeb()->Tib.StackBase */
                   "leaq -0x1000(%rax),%rsp\n\t"
                   /* attach dlls */
                   "call " __ASM_NAME("attach_thread") "\n\t"
                   "movq %rax,%rsp\n\t"
                   /* clear the stack */
                   "andq $~0xfff,%rax\n\t"  /* round down to page size */
                   "movq %rax,%rdi\n\t"
                   "call " __ASM_NAME("virtual_clear_thread_stack") "\n\t"
                   /* switch to the initial context */
                   "movq %rsp,%rdi\n\t"
                   "call " __ASM_NAME("set_cpu_context") )

extern void DECLSPEC_NORETURN call_thread_exit_func( int status, void (* HOSTPTR func)(int) );
__ASM_GLOBAL_FUNC( call_thread_exit_func,
                   /* fetch exit frame */
                   "movq %fs:0x1f4,%rdx\n\t"      /* x86_thread_data()->exit_frame */
                   "testq %rdx,%rdx\n\t"
                   "jnz 1f\n\t"
                   "jmp *%rsi\n"
                   /* switch to exit frame stack */
                   "1:\tmovq $0,%fs:0x1f4\n\t"
                   "movq %rdx,%rsp\n\t"
                   __ASM_CFI(".cfi_adjust_cfa_offset 56\n\t")
                   __ASM_CFI(".cfi_rel_offset %rbp,48\n\t")
                   __ASM_CFI(".cfi_rel_offset %rbx,40\n\t")
                   __ASM_CFI(".cfi_rel_offset %r12,32\n\t")
                   __ASM_CFI(".cfi_rel_offset %r13,24\n\t")
                   __ASM_CFI(".cfi_rel_offset %r14,16\n\t")
                   __ASM_CFI(".cfi_rel_offset %r15,8\n\t")
                   "call *%rsi" )


/***********************************************************************
 *           init_thread_context
 */
static void init_thread_context( CONTEXT *context, LPTHREAD_START_ROUTINE entry, void *arg, void *relay )
{
    context->SegCs  = wine_32on64_cs32;
    context->SegDs  = wine_get_ds();
    context->SegEs  = wine_get_es();
    context->SegFs  = wine_get_fs();
    context->SegGs  = wine_get_gs();
    context->SegSs  = wine_get_ss();
    context->EFlags = 0x202;
    context->Eax    = (DWORD)entry;
    context->Ebx    = (DWORD)arg;
    context->Esp    = (DWORD)NtCurrentTeb()->Tib.StackBase - 16;
    context->Eip    = (DWORD)relay;
    context->FloatSave.ControlWord = 0x27f;
    ((XMM_SAVE_AREA32 *)context->ExtendedRegisters)->ControlWord = 0x27f;
}


/***********************************************************************
 *           attach_thread
 */
PCONTEXT DECLSPEC_HIDDEN attach_thread( LPTHREAD_START_ROUTINE entry, void *arg,
                                        BOOL suspend, void *relay )
{
    CONTEXT *ctx;

    if (suspend)
    {
        CONTEXT context = { CONTEXT_ALL };

        init_thread_context( &context, entry, arg, relay );
        wait_suspend( &context );
        ctx = (CONTEXT *)((ULONG_PTR)context.Esp & ~15) - 1;
        *ctx = context;
    }
    else
    {
        ctx = (CONTEXT *)((char *)NtCurrentTeb()->Tib.StackBase - 16) - 1;
        init_thread_context( ctx, entry, arg, relay );
    }
    ctx->ContextFlags = CONTEXT_FULL;
    LdrInitializeThunk( ctx, (void **)&ctx->Eax, 0, 0 );
    return ctx;
}


/***********************************************************************
 *           signal_start_thread
 *
 * Thread startup sequence:
 * signal_start_thread()
 *   -> start_thread()
 *     -> call_thread_func_wrapper()
 *       -> call_thread_func()
 */
void signal_start_thread( LPTHREAD_START_ROUTINE entry, void *arg, BOOL suspend )
{
    start_thread( entry, arg, suspend, call_thread_func_wrapper );
}

/**********************************************************************
 *		signal_start_process
 *
 * Process startup sequence:
 * signal_start_process()
 *   -> start_thread()
 *     -> kernel32_start_process()
 */
void signal_start_process( LPTHREAD_START_ROUTINE entry, BOOL suspend )
{
    start_thread( entry, NtCurrentTeb()->Peb, suspend, kernel32_start_process );
}


/***********************************************************************
 *           signal_exit_thread
 */
void signal_exit_thread( int status )
{
    call_thread_exit_func( status, exit_thread );
}

/***********************************************************************
 *           signal_exit_process
 */
void signal_exit_process( int status )
{
    call_thread_exit_func( status, exit );
}

/**********************************************************************
 *		DbgBreakPoint   (NTDLL.@)
 */
__ASM_STDCALL_FUNC( DbgBreakPoint, 0, "int $3; ret")
__ASM_THUNK_STDCALL( DbgBreakPoint, 0, "int $3; ret")

/**********************************************************************
 *		DbgUserBreakPoint   (NTDLL.@)
 */
__ASM_STDCALL_FUNC( DbgUserBreakPoint, 0, "int $3; ret")
__ASM_THUNK_STDCALL( DbgUserBreakPoint, 0, "int $3; ret")

/**********************************************************************
 *           NtCurrentTeb   (NTDLL.@)
 */
__ASM_STDCALL_FUNC( NTDLL_NtCurrentTeb, 0, "movl %fs:0x18,%eax\n\tret" )
__ASM_THUNK_STDCALL( NTDLL_NtCurrentTeb, 0, "movl %fs:0x18,%eax\n\tret" )

/**************************************************************************
 *           _chkstk   (NTDLL.@)
 */
__ASM_STDCALL_FUNC32( __ASM_THUNK_NAME(_chkstk), 0,
                     "negl %eax\n\t"
                     "addl %esp,%eax\n\t"
                     "xchgl %esp,%eax\n\t"
                     "movl 0(%eax),%eax\n\t"  /* copy return address from old location */
                     "movl %eax,0(%esp)\n\t"
                     "ret" )

/**************************************************************************
 *           _alloca_probe   (NTDLL.@)
 */
__ASM_STDCALL_FUNC32( __ASM_THUNK_NAME(_alloca_probe), 0,
                     "negl %eax\n\t"
                     "addl %esp,%eax\n\t"
                     "xchgl %esp,%eax\n\t"
                     "movl 0(%eax),%eax\n\t"  /* copy return address from old location */
                     "movl %eax,0(%esp)\n\t"
                     "ret" )

/**********************************************************************
 *		EXC_CallHandler   (internal)
 *
 * Some exception handlers depend on EBP to have a fixed position relative to
 * the exception frame.
 * Shrinker depends on (*1) doing what it does,
 * (*2) being the exact instruction it is and (*3) beginning with 0x64
 * (i.e. the %fs prefix to the movl instruction). It also depends on the
 * function calling the handler having only 5 parameters (*4).
 */
__ASM_GLOBAL_FUNC32( __ASM_THUNK_NAME(EXC_CallHandler),
                  "pushl %ebp\n\t"
                  __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
                  __ASM_CFI(".cfi_rel_offset %ebp,0\n\t")
                  "movl %esp,%ebp\n\t"
                  __ASM_CFI(".cfi_def_cfa_register %ebp\n\t")
                   "pushl %ebx\n\t"
                   __ASM_CFI(".cfi_rel_offset %ebx,-4\n\t")
                   "movl 28(%ebp), %edx\n\t" /* ugly hack to pass the 6th param needed because of Shrinker */
                   "pushl 24(%ebp)\n\t"
                   "pushl 20(%ebp)\n\t"
                   "pushl 16(%ebp)\n\t"
                   "pushl 12(%ebp)\n\t"
                   "pushl 8(%ebp)\n\t"
                   "call " __ASM_NAME("call_exception_handler") "\n\t"
                   "popl %ebx\n\t"
                   __ASM_CFI(".cfi_same_value %ebx\n\t")
                   "leave\n"
                   __ASM_CFI(".cfi_def_cfa %esp,4\n\t")
                   __ASM_CFI(".cfi_same_value %ebp\n\t")
                   "ret" )
__ASM_GLOBAL_FUNC32(call_exception_handler,
                  "pushl %ebp\n\t"
                  __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
                  __ASM_CFI(".cfi_rel_offset %ebp,0\n\t")
                  "movl %esp,%ebp\n\t"
                  __ASM_CFI(".cfi_def_cfa_register %ebp\n\t")
                  "subl $12,%esp\n\t"
                  "pushl 12(%ebp)\n\t"      /* make any exceptions in this... */
                  "pushl %edx\n\t"          /* handler be handled by... */
                  ".byte 0x64\n\t"
                  "pushl (0)\n\t"           /* nested_handler (passed in edx). */
                  ".byte 0x64\n\t"
                  "movl %esp,(0)\n\t"       /* push the new exception frame onto the exception stack. */
                  "pushl 20(%ebp)\n\t"
                  "pushl 16(%ebp)\n\t"
                  "pushl 12(%ebp)\n\t"
                  "pushl 8(%ebp)\n\t"
                  "movl 24(%ebp), %ecx\n\t" /* (*1) */
                  "call *%ecx\n\t"          /* call handler. (*2) */
                  ".byte 0x64\n\t"
                  "movl (0), %esp\n\t"      /* restore previous... (*3) */
                  ".byte 0x64\n\t"
                  "popl (0)\n\t"            /* exception frame. */
                  "movl %ebp, %esp\n\t"     /* restore saved stack, in case it was corrupted */
                  "popl %ebp\n\t"
                   __ASM_CFI(".cfi_def_cfa %esp,4\n\t")
                   __ASM_CFI(".cfi_same_value %ebp\n\t")
                  "ret $20" )            /* (*4) */

#endif  /* __i386_on_x86_64__ */
