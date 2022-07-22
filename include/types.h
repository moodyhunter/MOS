// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#if __SIZE_WIDTH__ == 64
#error "MOS does not support 64-bit architectures (for now)"
#endif

#include <stdbool.h>
#include <stddef.h>

typedef char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef long double f80;

typedef u32 pid_t;
typedef u32 uid_t;
typedef u32 gid_t;
typedef u32 mode_t;
typedef u32 dev_t;
typedef u32 ino_t;

typedef s8 int8_t;
typedef u8 uint8_t;
typedef s16 int16_t;
typedef u16 uint16_t;
typedef s32 int32_t;
typedef u32 uint32_t;
typedef s64 int64_t;
typedef u64 uint64_t;

typedef u8 uchar;
typedef s8 schar;

typedef u32 uintptr_t;
typedef u32 uintmax_t;

_Static_assert(__SIZEOF_POINTER__ == sizeof(uintptr_t), "uintptr_t is not the same size as a pointer");
