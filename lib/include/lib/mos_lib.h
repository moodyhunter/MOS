// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

/**
 * \defgroup libs MOS Libraries
 * \brief A platform-independent library of useful data structures and functions.
 * @{
 */

#ifdef __MOS_KERNEL__ // ! Kernel
#include "mos/printk.h"
#define MOS_LIB_ASSERT(cond)             MOS_ASSERT(cond)
#define MOS_LIB_ASSERT_X(cond, msg, ...) MOS_ASSERT_X(cond, msg, ##__VA_ARGS__)
#define MOS_LIB_UNIMPLEMENTED(content)   MOS_UNIMPLEMENTED(content)
#define MOS_LIB_UNREACHABLE()            MOS_UNREACHABLE()
#else // ! Userspace
#define MOS_LIB_ASSERT(cond) MOS_UNUSED(cond)
#define MOS_LIB_ASSERT_X(cond, msg, ...)                                                                                                                                 \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        /* unimplemented */                                                                                                                                              \
        MOS_UNUSED(cond);                                                                                                                                                \
        MOS_UNUSED(msg);                                                                                                                                                 \
    } while (0)
#define MOS_LIB_UNIMPLEMENTED(content) ; /* unimplemented */
#define MOS_LIB_UNREACHABLE()          ; /* unimplemented */
#define mos_warn(...)                  ; /* unimplemented */
#define mos_panic(...)                 ; /* unimplemented */
#endif

/** @} */
