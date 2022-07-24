// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#define MOS_STRINGIFY2(x) #x
#define MOS_STRINGIFY(x) MOS_STRINGIFY2(x)
#define MOS_SOURCE_LOC __FILE__ ":" MOS_STRINGIFY(__LINE__)

#define panic(msg) kpanic(msg, MOS_SOURCE_LOC)

#define MOS_UNIMPLEMENTED(content) panic("UNIMPLEMENTED: " content)
#define MOS_UNREACHABLE() panic("UNREACHABLE")

// clang-format off
#define MOS_ASSERT(cond)        do { if (!(cond)) panic("Assertion failed: " #cond); } while (0)
// clang-format on
