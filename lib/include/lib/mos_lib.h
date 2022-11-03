// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

#ifdef __MOS_KERNEL__
#include "mos/printk.h"
#define MOS_LIB_ASSERT(cond)             MOS_ASSERT(cond)
#define MOS_LIB_ASSERT_X(cond, msg, ...) MOS_ASSERT_X(cond, msg, ##__VA_ARGS__)
#define MOS_LIB_UNIMPLEMENTED(content)   MOS_UNIMPLEMENTED(content)
#define MOS_LIB_UNREACHABLE()            MOS_UNREACHABLE()
#else
#define MOS_LIB_ASSERT(cond) MOS_UNUSED(cond)
#define MOS_LIB_ASSERT_X(cond, msg, ...)                                                                                                                                 \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        MOS_UNUSED(cond);                                                                                                                                                \
        MOS_UNUSED(msg);                                                                                                                                                 \
    } while (0)
#define MOS_LIB_UNIMPLEMENTED(content) MOS_UNUSED(content)
#define MOS_LIB_UNREACHABLE()
#define mos_warn(...)
inline void *liballoc_malloc(size_t size)
{
    (void) size;
    return (void *) 0;
}
inline void liballoc_free(const void *ptr)
{
    (void) ptr;
}
#define mos_panic(...)
#endif
