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
