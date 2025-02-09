// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/misc/panic.hpp"

#include <mos/mos_global.h>
#include <mos/types.hpp>

#define MOS_UNIMPLEMENTED(content)  mos_panic("\nUNIMPLEMENTED: %s", content)
#define MOS_UNREACHABLE()           mos_panic("\nUNREACHABLE line %d reached in file: %s", __LINE__, __FILE__)
#define MOS_UNREACHABLE_X(msg, ...) mos_panic("\nUNREACHABLE line %d reached in file: %s\n" msg, __LINE__, __FILE__, ##__VA_ARGS__)
#define MOS_ASSERT_ONCE(...)        MOS_ASSERT_X(once(), __VA_ARGS__)
#define MOS_ASSERT(cond)            MOS_ASSERT_X(cond, "")
#define MOS_ASSERT_X(cond, msg, ...)                                                                                                                                     \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (unlikely(!(cond)))                                                                                                                                           \
            mos_panic_inline("Assertion failed: %s\n" msg, #cond, ##__VA_ARGS__);                                                                                        \
    } while (0)

// these two also invokes a warning/panic handler
#define mos_warn(fmt, ...) mos_kwarn(__func__, __LINE__, "WARN: " fmt "\r\n", ##__VA_ARGS__)

#define mos_warn_once(...)                                                                                                                                               \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (once())                                                                                                                                                      \
            mos_warn(__VA_ARGS__);                                                                                                                                       \
    } while (0)

#define spinlock_assert_locked(lock) MOS_ASSERT(spinlock_is_locked(lock))

__printf(3, 4) void mos_kwarn(const char *func, u32 line, const char *fmt, ...);
