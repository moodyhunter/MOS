// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/kconfig.h"

#define MOS_DO_PRAGMA(x) _Pragma(#x)

#if defined(__clang__)
#define MOS_COMPILER_CLANG 1
#define MOS_PRAGMA(text)   MOS_DO_PRAGMA(clang text)
#elif defined(__GNUC__)
#define MOS_COMPILER_GCC 1
#define MOS_PRAGMA(text) MOS_DO_PRAGMA(GCC text)
#endif

#if MOS_COMPILER_GCC && __GNUC__ < 12
#define MOS_FILE_LOCATION __FILE__ ":" MOS_STRINGIFY(__LINE__)
#else
#define MOS_FILE_LOCATION __FILE_NAME__ ":" MOS_STRINGIFY(__LINE__)
#endif

#ifndef __cplusplus
#define static_assert _Static_assert
#endif

#define MOS_LITTLE_ENDIAN 1

static_assert(sizeof(long) == sizeof(void *), "long is not the same size as a pointer");
static_assert(sizeof(long) == MOS_BITS / 8, "long is not the same size as the architecture");
