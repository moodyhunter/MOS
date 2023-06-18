// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <limits.h>
#include <mos/mos_global.h>
#include <stdbool.h>
#include <stddef.h>

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long slong;
typedef signed long long int s64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long ulong;
typedef unsigned long long u64;

typedef unsigned long ptr_t;

typedef signed long intn;
typedef unsigned long uintn;

// native integer type
#if MOS_BITS == 32
#define PTR_FMT   "0x%8.8lx"
#define PTR_VLFMT "0x%lx"
#else
#define PTR_FMT   "0x%16.16lx"
#define PTR_VLFMT "0x%lx"
#endif

#define PTR_RANGE   "[" PTR_FMT " - " PTR_FMT "]"
#define PTR_RANGE64 "[0x%.16llx - 0x%.16llx]"

// physical frame number
typedef unsigned long long pfn_t;
#define PFN_FMT   "%llu"
#define PFN_RANGE "[" PFN_FMT " - " PFN_FMT "]"

MOS_STATIC_ASSERT(sizeof(void *) == MOS_BITS / 8, "pointer size check failed");
MOS_STATIC_ASSERT(sizeof(ptr_t) == sizeof(void *), "ptr_t is not the same size as void *");

typedef uintn reg_t; // native register type
typedef u16 reg16_t; // 16-bit
typedef u32 reg32_t; // 32-bit
typedef u64 reg64_t; // 64-bit

typedef union
{
    struct
    {
        bool b0 : 1;
        bool b1 : 1;
        bool b2 : 1;
        bool b3 : 1;
        bool b4 : 1;
        bool b5 : 1;
        bool b6 : 1;
        bool msb : 1;
    } __packed bits;
    u8 byte;
} byte_t;

MOS_STATIC_ASSERT(sizeof(byte_t) == 1, "byte_t is not 1 byte");

typedef u32 id_t;
typedef u32 uid_t;
typedef u32 gid_t;

typedef s32 pid_t;
typedef s32 tid_t;

typedef s32 fd_t;

typedef signed long ssize_t;
typedef ssize_t off_t;

typedef long pte_content_t;

#define new_named_opaque_type(base, name, type)                                                                                                                          \
    typedef struct                                                                                                                                                       \
    {                                                                                                                                                                    \
        base name;                                                                                                                                                       \
    } type

#define new_opaque_type(type, name) new_named_opaque_type(type, name, name##_t)
#define new_opaque_ptr_type(name)   new_named_opaque_type(ptr_t, ptr, name)

new_opaque_type(size_t, hash);

typedef u32 futex_word_t;

#ifndef __cplusplus
#define __atomic(type) _Atomic(type)
typedef __atomic(size_t) atomic_t;
#endif

typedef void (*thread_entry_t)(void *arg);
