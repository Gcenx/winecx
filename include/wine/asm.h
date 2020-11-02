/*
 * Inline assembly support
 *
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

#ifndef __WINE_WINE_ASM_H
#define __WINE_WINE_ASM_H

#if defined(__APPLE__) || (defined(_WIN32) && (defined(__i386__) || defined(__i386_on_x86_64__)))
# define __ASM_NAME(name) "_" name
#else
# define __ASM_NAME(name) name
#endif

#if defined(_WIN32) && (defined(__i386__) || defined(__i386_on_x86_64__))
# define __ASM_STDCALL(name,args) __ASM_NAME(name) "@" #args
#else
# define __ASM_STDCALL(name,args) __ASM_NAME(name)
#endif

#if defined(__GCC_HAVE_DWARF2_CFI_ASM) || defined(__APPLE__)
# define __ASM_CFI(str) str
#else
# define __ASM_CFI(str)
#endif

#ifdef __SEH__
# define __ASM_SEH(str) str
#else
# define __ASM_SEH(str)
#endif

#ifdef _WIN32
# define __ASM_FUNC_TYPE(name) ".def " name "; .scl 2; .type 32; .endef"
#elif defined(__APPLE__)
# define __ASM_FUNC_TYPE(name) ""
#elif defined(__arm__) || defined(__arm64__)
# define __ASM_FUNC_TYPE(name) ".type " name ",%function"
#else
# define __ASM_FUNC_TYPE(name) ".type " name ",@function"
#endif

#ifdef __GNUC__
# define __ASM_DEFINE_FUNC(name,code) \
    asm(".text\n\t.align 4\n\t.globl " name "\n\t" __ASM_FUNC_TYPE(name) __ASM_SEH("\n\t.seh_proc " name) "\n" name ":\n\t" \
        __ASM_CFI(".cfi_startproc\n\t") code __ASM_CFI("\n\t.cfi_endproc") __ASM_SEH("\n\t.seh_endproc") );
#else
# define __ASM_DEFINE_FUNC(name,code) void __asm_dummy_##__LINE__(void) { \
    asm(".text\n\t.align 4\n\t.globl " name "\n\t" __ASM_FUNC_TYPE(name) __ASM_SEH("\n\t.seh_proc " name) "\n" name ":\n\t" \
        __ASM_CFI(".cfi_startproc\n\t") code __ASM_CFI("\n\t.cfi_endproc") __ASM_SEH("\n\t.seh_endproc") ); }
#endif

#define __ASM_GLOBAL_FUNC(name,code) __ASM_DEFINE_FUNC(__ASM_NAME(#name),code)

#define __ASM_STDCALL_FUNC(name,args,code) __ASM_DEFINE_FUNC(__ASM_STDCALL(#name,args),code)

/* fastcall support */

#if defined(__i386__) && !defined(_WIN32)

# define DEFINE_FASTCALL1_WRAPPER(func) \
    __ASM_STDCALL_FUNC( __fastcall_ ## func, 4, \
                        "popl %eax\n\t"  \
                        "pushl %ecx\n\t" \
                        "pushl %eax\n\t" \
                        "jmp " __ASM_STDCALL(#func,4) )
# define DEFINE_FASTCALL_WRAPPER(func,args) \
    __ASM_STDCALL_FUNC( __fastcall_ ## func, args, \
                        "popl %eax\n\t"  \
                        "pushl %edx\n\t" \
                        "pushl %ecx\n\t" \
                        "pushl %eax\n\t" \
                        "jmp " __ASM_STDCALL(#func,args) )

#else  /* __i386__ */

# define DEFINE_FASTCALL1_WRAPPER(func) /* nothing */
# define DEFINE_FASTCALL_WRAPPER(func,args) /* nothing */

#endif  /* __i386__ */

/* thiscall support */

#if defined(__i386__) && !defined(__MINGW32__)

# ifdef _MSC_VER
#  define DEFINE_THISCALL_WRAPPER(func,args) \
    __declspec(naked) void __thiscall_##func(void) \
    { __asm { \
        pop eax \
        push ecx \
        push eax \
        jmp func \
    } }
# else  /* _MSC_VER */
#  define DEFINE_THISCALL_WRAPPER(func,args) \
    extern void __thiscall_ ## func(void);  \
    __ASM_STDCALL_FUNC( __thiscall_ ## func, args, \
                       "popl %eax\n\t"  \
                       "pushl %ecx\n\t" \
                       "pushl %eax\n\t" \
                        "jmp " __ASM_STDCALL(#func,args) )
# endif  /* _MSC_VER */

# define THISCALL(func) (void *)__thiscall_ ## func
# define THISCALL_NAME(func) __ASM_NAME("__thiscall_" #func)

#else  /* __i386__ */

# define DEFINE_THISCALL_WRAPPER(func,args) /* nothing */
# define THISCALL(func) func
# define THISCALL_NAME(func) __ASM_NAME(#func)

#endif  /* __i386__ */

/* 32-to-64 thunk support */

#ifdef __i386_on_x86_64__

#include <windef.h>

#define __ASM_EXTRA_DIST "16"

#define __ASM_STR__(name)    #name
#define __ASM_STR(name)      __ASM_STR__(name)

#define __ASM_THUNK_PREFIX                            wine
#define __ASM_THUNK_MAKE_NAME__(thunk_prefix,name)    thunk_prefix##_thunk_##name
#define __ASM_THUNK_MAKE_NAME(thunk_prefix,name)      __ASM_THUNK_MAKE_NAME__(thunk_prefix, name)
#define __ASM_THUNK_NAME(name)                        __ASM_THUNK_MAKE_NAME(__ASM_THUNK_PREFIX, name)
#define __ASM_THUNK_SYMBOL(name_str)                  __ASM_NAME(__ASM_STR(__ASM_THUNK_PREFIX) "_thunk_" name_str)
#define __ASM_THUNK_STDCALL_SYMBOL(name_str,args)     __ASM_STDCALL(__ASM_STR(__ASM_THUNK_PREFIX) "_thunk_" name_str, args)
#define __ASM_THUNK_MAGIC                             0x77496e4554683332
#define __ASM_THUNK_TARGET(thunk_addr)                ((void *)(*((unsigned long *)thunk_addr - 2) + (unsigned long)thunk_addr + 12))

#define __ASM_THUNK_DEFINE(name,code)                                                        \
asm(".text\n\t"                                                                              \
    ".balign 32\n\t"                                                                         \
    ".quad " __ASM_NAME(name) " - (" __ASM_THUNK_SYMBOL(name) " + 12)\n\t"                   \
    ".quad " __ASM_STR(__ASM_THUNK_MAGIC) "\n\t"                                             \
    ".globl " __ASM_THUNK_SYMBOL(name) "\n\t"                                                \
    __ASM_FUNC_TYPE(__ASM_THUNK_SYMBOL(name)) "\n"                                           \
    __ASM_THUNK_SYMBOL(name) ":\n\t"                                                         \
    ".cfi_startproc\n\t"                                                                     \
    ".code32\n\t"                                                                            \
    ".byte 0x8b, 0xff\n\t" /* movl %edi, %edi; hotpatch prolog */                            \
    code "\n\t"                                                                              \
    ".code64\n\t"                                                                            \
    ".cfi_endproc\n\t"                                                                       \
    ".previous");
#define __ASM_THUNK_STDCALL(name,args,code)    __ASM_THUNK_DEFINE(#name,code)
#define __ASM_THUNK_GLOBAL(name,code)          __ASM_THUNK_DEFINE(#name,code)
#define __ASM_STDCALL_FUNC32(name,args,code)   __ASM_STDCALL_FUNC(name,args,".code32\n\t" code "\n\t.code64\n\t")
#define __ASM_GLOBAL_FUNC32(name,code)         __ASM_GLOBAL_FUNC(name,".code32\n\t" code "\n\t.code64\n\t")

#define WINE_CALL_IMPL32(name) ({ typeof(name) * volatile p_##name = name; p_##name; })

static inline BOOL wine_is_thunk32to64(void *func)
{
    /* Function address must be an odd multiple of 16 bytes; function must begin with
       hotpatchable prolog; function must be preceded by magic number. */
    unsigned long *thunk_magic = (unsigned long *)func - 1;
    return (((unsigned long)func & 0x1f) == 0x10 && *(WORD*)func == 0xff8b && *thunk_magic == __ASM_THUNK_MAGIC);
}

#endif /* __i386_on_x86_64__ */

#endif  /* __WINE_WINE_ASM_H */
