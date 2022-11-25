// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

/**
 * \defgroup libs MOS Libraries
 * \brief A platform-independent library of useful data structures and functions.
 * @{
 */

#ifdef __MOS_KERNEL__ // ! Kernel
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"
#define MOS_LIB_ASSERT(cond)             MOS_ASSERT(cond)
#define MOS_LIB_ASSERT_X(cond, msg, ...) MOS_ASSERT_X(cond, msg, ##__VA_ARGS__)
#define MOS_LIB_UNIMPLEMENTED(content)   MOS_UNIMPLEMENTED(content)
#define MOS_LIB_UNREACHABLE()            MOS_UNREACHABLE()
should_inline void *mos_lib_malloc(size_t size)
{
    return kmalloc(size);
}
should_inline void mos_lib_free(void *ptr)
{
    kfree(ptr);
}
should_inline void *mos_lib_realloc(void *ptr, size_t size)
{
    return krealloc(ptr, size);
}
should_inline void *mos_lib_calloc(size_t nmemb, size_t size)
{
    return kcalloc(nmemb, size);
}
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
should_inline void *mos_lib_malloc(size_t size)
{
    MOS_UNUSED(size);
    return NULL; // unimplemented
}
should_inline void mos_lib_free(void *ptr)
{
    MOS_UNUSED(ptr);
    // unimplemented
}
should_inline void *mos_lib_realloc(void *ptr, size_t size)
{
    MOS_UNUSED(ptr);
    MOS_UNUSED(size);
    return NULL; // unimplemented
}
should_inline void *mos_lib_calloc(size_t nmemb, size_t size)
{
    MOS_UNUSED(nmemb);
    MOS_UNUSED(size);
    return NULL; // unimplemented
}
#endif

/** @} */
