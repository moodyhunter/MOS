// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "panic.h"

#define MOS_ASSERT(cond)                                                                                                                        \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (!(cond))                                                                                                                            \
            panic("Assertion failed: " #cond);                                                                                                  \
    } while (0)

#define MOS_UNIMPLEMENTED(content) panic("UNIMPLEMENTED: " content)
#define MOS_UNREACHABLE()          panic("UNREACHABLE")
#define MOS_TODO(msg)              warning("TODO: " msg)
