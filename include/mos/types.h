// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"

#include <stdbool.h>
#include <stddef.h>

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef signed long long int s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef long double f80;

typedef unsigned long uintptr_t;
static_assert(sizeof(uintptr_t) == sizeof(void *), "uintptr_t is not the same size as a pointer");

// reg_t represents a register value
typedef unsigned long reg_t;
typedef u16 reg16_t;
typedef u32 reg32_t;
typedef u64 reg64_t;

#ifdef MOS_32BITS
#define PTR_FMT "0x%8.8lx"
#elif defined(MOS_64BITS)
#define PTR_FMT "0x%16.16lx"
#else
#error "Something is wrong with the architecture"
#endif

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

static_assert(sizeof(byte_t) == 1, "byte_t is not 1 byte");

typedef u32 id_t;
typedef s32 fd_t;

typedef signed long ssize_t;

typedef id_t uid_t;
typedef id_t gid_t;
typedef id_t pid_t;
typedef id_t tid_t;

// clang-format off
#define _named_opaque_type(base, name, type) typedef struct { base name; } __packed type
// clang-format on

#define new_opaque_type(type, name) _named_opaque_type(type, name, name##_t)
#define new_opaque_ptr_type(name)   _named_opaque_type(uintptr_t, ptr, name)

new_opaque_ptr_type(paging_handle_t);
new_opaque_type(u64, atomic);
new_opaque_type(size_t, hash);

#undef _named_opaque_type
#undef new_opaque_type
#undef new_opaque_ptr_type
