// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"

#include <limits.h>
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

typedef float f32;
typedef double f64;
typedef long double f80;

// native integer type
#if MOS_BITS == 32
typedef s32 intn;
typedef u32 uintn;
#else
typedef s64 intn;
typedef u64 uintn;
#endif

// native register type
typedef uintn reg_t;

// reg_t represents a register value
typedef u16 reg16_t;
typedef u32 reg32_t;
typedef u64 reg64_t;

#if MOS_BITS == 32
#define PTR_FMT "0x%8.8lx"
#else
#define PTR_FMT "0x%16.16llx"
#endif

typedef unsigned long uintptr_t;
MOS_STATIC_ASSERT(sizeof(uintptr_t) == sizeof(void *), "uintptr_t is not the same size as void *");

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

typedef signed long id_t;
typedef id_t uid_t;
typedef id_t pid_t;
typedef id_t fd_t;
typedef id_t gid_t;
typedef id_t tid_t;

typedef signed long ssize_t;
typedef ssize_t off_t;

// clang-format off
#define _named_opaque_type(base, name, type) typedef struct { base name; } __packed type
// clang-format on

#define new_opaque_type(type, name) _named_opaque_type(type, name, name##_t)
#define new_opaque_ptr_type(name)   _named_opaque_type(uintptr_t, ptr, name)

new_opaque_type(size_t, hash);

#undef _named_opaque_type
#undef new_opaque_type
#undef new_opaque_ptr_type

typedef u32 futex_word_t;

#ifndef __cplusplus
#define __atomic(type) _Atomic(type)
typedef __atomic(size_t) atomic_t;
#endif

typedef void (*thread_entry_t)(void *arg);
