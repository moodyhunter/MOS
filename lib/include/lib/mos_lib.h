// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

/**
 * @defgroup libs MOS Libraries
 * @brief A platform-independent library of useful data structures and functions.
 * @{
 */

#ifdef __MOS_KERNEL__ // ! Kernel
#include "mos/printk.h"
#define MOS_LIB_ASSERT(cond)             MOS_ASSERT(cond)
#define MOS_LIB_ASSERT_X(cond, msg, ...) MOS_ASSERT_X(cond, msg, ##__VA_ARGS__)
#define MOS_LIB_UNIMPLEMENTED(content)   MOS_UNIMPLEMENTED(content)
#define MOS_LIB_UNREACHABLE()            MOS_UNREACHABLE()
#else // ! Userspace
#include "libuserspace.h"
#define MOS_LIB_ASSERT(cond)                                                                                                                                             \
    if (!(cond))                                                                                                                                                         \
    fatal_abort("Assertion failed: %s", #cond)
#define MOS_LIB_ASSERT_X(cond, msg, ...)                                                                                                                                 \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (!(cond))                                                                                                                                                     \
            fatal_abort("Assertion failed: %s: " msg, #cond, ##__VA_ARGS__);                                                                                             \
    } while (0)
#define MOS_LIB_UNIMPLEMENTED(content) fatal_abort("Unimplemented: " content)
#define MOS_LIB_UNREACHABLE()          fatal_abort("Unreachable code reached")
#define mos_warn(...)                  dprintf(stderr, __VA_ARGS__)
#define mos_panic(...)                 fatal_abort(__VA_ARGS__)
#endif

/** @} */
