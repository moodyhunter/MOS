// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#define MOS_DO_PRAGMA(x) _Pragma(#x)

#if defined(__clang__)
#define MOS_COMPILER_CLANG 1
#define MOS_PRAGMA(text)   MOS_DO_PRAGMA(clang text)
#elif defined(__GNUC__)
#define MOS_COMPILER_GCC 1
#define MOS_PRAGMA(text) MOS_DO_PRAGMA(GCC text)
#endif

#if __SIZEOF_POINTER__ == 4
#define MOS_32BIT 1
#define MOS_BITS  32
#elif __SIZEOF_POINTER__ == 8
#define MOS_64BIT 1
#define MOS_BITS  64
#else
#error "Unknown pointer size"
#endif

#ifndef __cplusplus
#define static_assert _Static_assert
#endif

static_assert(__SIZEOF_POINTER__ == sizeof(void *), "uintptr_t is not the same size as a pointer");
