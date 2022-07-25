// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifdef __cplusplus
#define noreturn __attribute__((__noreturn__))
#else
#include <stdnoreturn.h>
#define static_assert _Static_assert
#endif

#define __attr_packed     __attribute__((__packed__))
#define __attr_aligned(x) __attribute__((__aligned__(x)))

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
