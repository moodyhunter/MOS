// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/compiler.h"
#include "mos/kconfig.h"

#define __packed     __attribute__((__packed__))
#define __aligned(x) __attribute__((__aligned__(x)))
#define __used       __attribute__((__used__))
#define __unused     __attribute__((__unused__))
#define __section(S) __attribute__((__section__(#S)))

// function attributes
#define __printf(a, b)  __attribute__((__format__(__printf__, a, b)))
#define __malloc        __attribute__((__malloc__))
#define __noreturn      __attribute__((__noreturn__))
#define __always_inline __attribute__((__always_inline__))
#define __cold          __attribute__((__cold__))

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define to_union(u) __extension__(u)

#define B
#define KB B * 1024
#define MB KB * 1024
#define GB MB * 1024

#define MOS_STRINGIFY2(x) #x
#define MOS_STRINGIFY(x)  MOS_STRINGIFY2(x)

#define MOS_UNUSED(x) (void) (x)

#define MOS_CONCAT_INNER(a, b) a##b
#define MOS_CONCAT(a, b)       MOS_CONCAT_INNER(a, b)

#define MOS_WARNING_PUSH          MOS_PRAGMA(diagnostic push)
#define MOS_WARNING_POP           MOS_PRAGMA(diagnostic pop)
#define MOS_WARNING_DISABLE(text) MOS_PRAGMA(diagnostic ignored text)
