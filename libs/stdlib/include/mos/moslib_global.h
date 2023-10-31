// SPDX-License-Identifier: GPL-3.0-or-later
// Preamble for MOS standard library headers.

#pragma once

#include <mos/types.h>

/**
 * @defgroup libs MOS Libraries
 * @brief A platform-independent library of useful data structures and functions.
 * @{
 */

#define MOS_LIB_ASSERT(cond) MOS_LIB_ASSERT_X(cond, "")

#ifdef __MOS_KERNEL__ // ! Kernel
#define MOS_LIB_ASSERT_X(cond, msg, ...) MOS_ASSERT_X(cond, msg, ##__VA_ARGS__)
#define MOS_LIB_UNIMPLEMENTED(content)   MOS_UNIMPLEMENTED(content)
#define MOS_LIB_UNREACHABLE()            MOS_UNREACHABLE()

#include "mos/printk.h"

#else // ! Userspace

#include <mos/platform_syscall.h>
#include <mos/syscall/usermode.h>

#define MOS_LIB_ASSERT_X(cond, msg, ...)                                                                                                                                 \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (unlikely(!(cond)))                                                                                                                                           \
            fatal_abort("Assertion failed: '%s', " msg "\n", #cond, ##__VA_ARGS__);                                                                                      \
    } while (0)

MOSAPI void __printf(1, 2) fatal_abort(const char *fmt, ...);

#define MOS_LIB_UNIMPLEMENTED(content) fatal_abort("Unimplemented: %s", content)
#define MOS_LIB_UNREACHABLE()          fatal_abort("Unreachable code reached")
#define mos_panic(fmt, ...)            fatal_abort(fmt "\n", ##__VA_ARGS__)
#define mos_warn(fmt, ...)             fprintf(stderr, "WARN: " fmt "\n", ##__VA_ARGS__)
#endif

/** @} */
