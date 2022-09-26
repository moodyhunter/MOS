// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/compiler.h"

#if MOS_32BITS
#define MOS_KERNEL_START_VADDR 0xC0000000
#endif

#if MOS_64BITS
#define MOS_KERNEL_START_VADDR 0xFFFFFFFFC0000000
#endif

// clang-format off
#define B  * 1
#define KB * 1024 B
#define MB * 1024 KB
#define GB * (u64) 1024 MB
#define TB * (u64) 1024 GB
// clang-format on
