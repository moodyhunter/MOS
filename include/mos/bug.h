// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "attributes.h"

void _kwarn_impl(const char *msg, const char *func, const char *file, const char *line);
noreturn void _kpanic_impl(const char *msg, const char *func, const char *file, const char *line);

#define MOS_STRINGIFY2(x) #x
#define MOS_STRINGIFY(x)  MOS_STRINGIFY2(x)

#define panic(msg)   _kpanic_impl(msg, __func__, __FILE__, MOS_STRINGIFY(__LINE__))
#define warning(msg) _kwarn_impl(msg, __func__, __FILE__, MOS_STRINGIFY(__LINE__))

#define MOS_ASSERT(cond)                                                                                                                        \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (!(cond))                                                                                                                            \
            panic("Assertion failed: " #cond);                                                                                                  \
    } while (0)

#define MOS_UNIMPLEMENTED(content) panic("UNIMPLEMENTED: " content)
#define MOS_UNREACHABLE()          panic("UNREACHABLE")
#define MOS_TODO(msg)              warning("TODO: " msg)
