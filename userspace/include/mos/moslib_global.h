// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

#define stdin  0
#define stdout 1
#define stderr 2

#ifdef __cplusplus
#define MOSAPI extern "C"
#else
#define MOSAPI extern
#endif

#define MOS_LIB_ASSERT(cond)                                                                                                                                             \
    if (!(cond))                                                                                                                                                         \
    fatal_abort("Assertion failed: %s", #cond)
#define MOS_LIB_ASSERT_X(cond, msg, ...)                                                                                                                                 \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (!(cond))                                                                                                                                                     \
            fatal_abort("Assertion failed: %s: " msg, #cond, ##__VA_ARGS__);                                                                                             \
    } while (0)

MOSAPI int __printf(2, 3) dprintf(int fd, const char *fmt, ...);
MOSAPI void __printf(1, 2) fatal_abort(const char *fmt, ...);

#define MOS_LIB_UNIMPLEMENTED(content) fatal_abort("Unimplemented: %s", content)
#define MOS_LIB_UNREACHABLE()          fatal_abort("Unreachable code reached")
#define mos_warn(fmt, ...)             dprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define mos_panic(fmt, ...)            fatal_abort(fmt "\n", ##__VA_ARGS__)
