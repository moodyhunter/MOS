// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifndef __cplusplus
#define static_assert _Static_assert
#endif

static_assert(__SIZEOF_POINTER__ == sizeof(void *), "uintptr_t is not the same size as a pointer");

#define __attr_noreturn      __attribute__((__noreturn__))
#define __attr_packed        __attribute__((__packed__))
#define __attr_aligned(x)    __attribute__((__aligned__(x)))
#define __attr_unused        __attribute__((__unused__))
#define __attr_used          __attribute__((__used__))
#define __attr_printf(a, b)  __attribute__((__format__(__printf__, a, b)))
#define __attr_always_inline __attribute__((__always_inline__))
#define __section(S)         __attribute__((__section__(#S)))

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define to_union(u) __extension__(u)

#define MOS_STRINGIFY2(x) #x
#define MOS_STRINGIFY(x)  MOS_STRINGIFY2(x)

#define MOS_UNUSED(x) (void) (x)

#define MOS_CONCAT_INNER(a, b) a##b
#define MOS_CONCAT(a, b)       MOS_CONCAT_INNER(a, b)

#define MOS_PRAGMA(text) _Pragma(#text)

#define MOS_GCC_DO_PRAGMA(text)   MOS_PRAGMA(GCC text)
#define MOS_WARNING_PUSH          MOS_GCC_DO_PRAGMA(diagnostic push)
#define MOS_WARNING_POP           MOS_GCC_DO_PRAGMA(diagnostic pop)
#define MOS_WARNING_DISABLE(text) MOS_GCC_DO_PRAGMA(diagnostic ignored text)
