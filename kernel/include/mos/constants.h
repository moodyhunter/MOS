// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/compiler.h"

#if MOS_BITS == 32
#define MOS_KERNEL_START_VADDR 0xC0000000
#elif MOS_BITS == 64
#define MOS_KERNEL_START_VADDR 0xFFFFFFFFC0000000
#else
#error "Cannot determine MOS_KERNEL_START_VADDR"
#endif

#define MOS_MAX_VADDR ((uintptr_t) ~0)

#define MOS_SYSCALL_INTR             0x88
#define BIOS_VADDR_MASK              0xF0000000
#define BIOS_VADDR(paddr)            (BIOS_VADDR_MASK | ((uintptr_t) (paddr)))
#define BIOS_VADDR_TYPE(paddr, type) ((type) BIOS_VADDR((paddr)))

// clang-format off
#define KB * 1024
#define MB * 1024 KB
#define GB * (u64) 1024 MB
#define TB * (u64) 1024 GB
// clang-format on
