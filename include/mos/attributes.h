// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifndef __cplusplus
#define static_assert _Static_assert
#endif

#define __attr_noreturn   __attribute__((__noreturn__))
#define __attr_packed     __attribute__((__packed__))
#define __attr_aligned(x) __attribute__((__aligned__(x)))
#define __attr_unused     __attribute__((__unused__))
#define __attr_used       __attribute__((__used__))

/* Simple shorthand for a section definition */
#ifndef __section
#define __section(S) __attribute__((__section__(#S)))
#endif

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define MOS_STRINGIFY2(x) #x
#define MOS_STRINGIFY(x)  MOS_STRINGIFY2(x)

#define MOS_CONCAT_INNER(a, b) a##b
#define MOS_CONCAT(a, b)       MOS_CONCAT_INNER(a, b)

#define MOS_PRAGMA(text) _Pragma(#text)

#define MOS_GCC_DO_PRAGMA(text)   MOS_PRAGMA(GCC text)
#define MOS_WARNING_PUSH          MOS_GCC_DO_PRAGMA(diagnostic push)
#define MOS_WARNING_POP           MOS_GCC_DO_PRAGMA(diagnostic pop)
#define MOS_WARNING_DISABLE(text) MOS_GCC_DO_PRAGMA(diagnostic ignored text)
