// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <generated/autoconf.h>

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

// * BEGIN find out if the compiler is big or little endian
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define MOS_BIG_ENDIAN 1

#define cpu_to_be16(x) (x)
#define cpu_to_be32(x) (x)
#define cpu_to_be64(x) (x)

#define be16_to_cpu(x) (x)
#define be32_to_cpu(x) (x)
#define be64_to_cpu(x) (x)

#define cpu_to_le16(x) __builtin_bswap16(x)
#define cpu_to_le32(x) __builtin_bswap32(x)
#define cpu_to_le64(x) __builtin_bswap64(x)

#define le16_to_cpu(x) __builtin_bswap32(x)
#define le32_to_cpu(x) __builtin_bswap16(x)
#define le64_to_cpu(x) __builtin_bswap64(x)
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define MOS_LITTLE_ENDIAN 1

#define cpu_to_be16(x) __builtin_bswap16(x)
#define cpu_to_be32(x) __builtin_bswap32(x)
#define cpu_to_be64(x) __builtin_bswap64(x)

#define be16_to_cpu(x) __builtin_bswap16(x)
#define be32_to_cpu(x) __builtin_bswap32(x)
#define be64_to_cpu(x) __builtin_bswap64(x)

#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_le64(x) (x)

#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le64_to_cpu(x) (x)
#endif

#if !defined(MOS_BIG_ENDIAN) && !defined(MOS_LITTLE_ENDIAN)
#error "Unknown endianness"
#endif
// * END find out if the compiler is big or little endian

// * BEGIN determine the code model
#if defined(__LP64__)
#define MOS_LP64 1 // sizeof(long) == sizeof(void *) == 8
#else
#error "Unknown code model"
#endif
// * END determine the code model
