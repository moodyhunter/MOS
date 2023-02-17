// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/kconfig.h"

#define MOS_DO_PRAGMA(x) _Pragma(#x)

#if defined(__clang__)
#define MOS_COMPILER_CLANG 1
#define MOS_PRAGMA(text)   MOS_DO_PRAGMA(clang text)
#elif defined(__GNUC__)
#define MOS_COMPILER_GCC 1
#define MOS_PRAGMA(text) MOS_DO_PRAGMA(GCC text)
#endif

#if MOS_COMPILER_GCC && __GNUC__ < 12
#define MOS_FILE_LOCATION __FILE__ ":" MOS_STRINGIFY(__LINE__)
#else
#define MOS_FILE_LOCATION __FILE_NAME__ ":" MOS_STRINGIFY(__LINE__)
#endif

#ifdef __cplusplus
#define MOS_STATIC_ASSERT static_assert
#else
#define MOS_STATIC_ASSERT _Static_assert
#endif

// find out if the compiler is big or little endian
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define MOS_BIG_ENDIAN 1
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define MOS_LITTLE_ENDIAN 1
#else
#error "Unknown endianness"
#endif

// find out if the code model is LP32, LP64 or LLP64
// LLP64
//      sizeof(long) == 4,
//      sizeof(long long) == sizeof(void *) == 8
// LP64
//      sizeof(long) == sizeof(long long) == sizeof(void *) == 8
// LP32
//      sizeof(long) == sizeof(long long) == sizeof(void *) == 4
//
// MOS only supports LP32 when MOS_BITS == 32, and both LP64 and LLP64 when MOS_BITS == 64

#if MOS_BITS == 32
#if __SIZEOF_LONG__ != 4
#error "Invalid code model"
#endif
#define MOS_LP32 1
#else // MOS_BITS == 64
#if __SIZEOF_LONG__ == __SIZEOF_LONG_LONG__ && __SIZEOF_LONG__ == __SIZEOF_POINTER__
#define MOS_LP64 1
#elif __SIZEOF_LONG__ == 4 && __SIZEOF_LONG_LONG__ == __SIZEOF_POINTER__ && __SIZEOF_LONG_LONG__ == 8
#define MOS_LLP64 1
#else
#error "Invalid code model"
#endif
#endif

MOS_STATIC_ASSERT(sizeof(void *) == MOS_BITS / 8, "pointer size check failed");
