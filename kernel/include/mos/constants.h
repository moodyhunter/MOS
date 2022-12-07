// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/compiler.h"

#if MOS_32BITS
#define MOS_KERNEL_START_VADDR 0xC0000000
#endif

#if MOS_64BITS
#define MOS_KERNEL_START_VADDR 0xFFFFFFFFC0000000
#endif

#define MOS_SYSCALL_INTR             0x88
#define BIOS_VADDR_MASK              0xE0000000
#define BIOS_VADDR(paddr)            (BIOS_VADDR_MASK | ((paddr) & ~0xF0000000))
#define BIOS_VADDR_TYPE(paddr, type) ((type) BIOS_VADDR((uintptr_t) (paddr)))

// clang-format off
#define KB * 1024
#define MB * 1024 KB
#define GB * (u64) 1024 MB
#define TB * (u64) 1024 GB
// clang-format on
