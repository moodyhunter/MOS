// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/compiler.h"
#include "mos/kconfig.h"

#define __aligned(x)    __attribute__((__aligned__(x)))
#define __cold          __attribute__((__cold__))
#define __malloc        __attribute__((__malloc__))
#define __packed        __attribute__((__packed__))
#define __printf(a, b)  __attribute__((__format__(__printf__, a, b)))
#define __pure          __attribute__((__pure__))
#define __section(S)    __attribute__((__section__(#S)))
#define __maybe_unused  __attribute__((__unused__))
#define __used          __attribute__((__used__))
#define __weak_alias(x) __attribute__((weak, alias(x)))
#define __weakref(x)    __attribute__((weakref(x)))

#define noreturn      __attribute__((__noreturn__))
#define asmlinkage    __attribute__((regparm(0)))
#define should_inline __maybe_unused static inline
#define always_inline should_inline __attribute__((__always_inline__))

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define to_union(u) __extension__(u)

// clang-format off
#define B  * 1
#define KB * 1024 B
#define MB * 1024 KB
#define GB * (u64) 1024 MB
#define TB * (u64) 1024 GB
// clang-format on

#define GET_BIT(x, n)               (((x) >> (n)) & 1)
#define MASK_BITS(value, width)     ((value) & ((1 << (width)) - 1))
#define SET_BITS(bit, width, value) (MASK_BITS(value, width) << (bit))

#define MOS_STRINGIFY2(x) #x
#define MOS_STRINGIFY(x)  MOS_STRINGIFY2(x)

#define MOS_UNUSED(x) (void) (x)

#define MOS_CONCAT_INNER(a, b) a##b
#define MOS_CONCAT(a, b)       MOS_CONCAT_INNER(a, b)

#define MOS_WARNING_PUSH          MOS_PRAGMA(diagnostic push)
#define MOS_WARNING_POP           MOS_PRAGMA(diagnostic pop)
#define MOS_WARNING_DISABLE(text) MOS_PRAGMA(diagnostic ignored text)

#if MOS_32BITS
#define MOS_KERNEL_START_VADDR 0xC0000000
#else
#define MOS_KERNEL_START_VADDR 0xFFFFFFFF80000000
#endif
