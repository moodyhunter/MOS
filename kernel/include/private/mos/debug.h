// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform_defs.h"

#ifndef MOS_PLATFORM_DEBUG_MODULES
#define MOS_PLATFORM_DEBUG_MODULES(X)
#endif

// clang-format off
// ! Keep this list consistent with kernel/Kconfig.debug
#define MOS_ALL_DEBUG_MODULES(X)    \
    MOS_PLATFORM_DEBUG_MODULES(X)   \
    X(cpio)         \
    X(dcache)       \
    X(dcache_ref)   \
    X(dma)          \
    X(elf)          \
    X(futex)        \
    X(io)           \
    X(ipc)          \
    X(ipi)          \
    X(panic)        \
    X(pagefault)    \
    X(pipe)         \
    X(pmm)          \
    X(pmm_buddy)    \
    X(process)      \
    X(scheduler)    \
    X(setup)        \
    X(signal)       \
    X(slab)         \
    X(spinlock)     \
    X(syscall)      \
    X(sysfs)        \
    X(thread)       \
    X(tmpfs)        \
    X(userfs)       \
    X(vfs)          \
    X(vmm)
// clang-format on

#if MOS_CONFIG(MOS_DYNAMIC_DEBUG)
extern struct
{
#define _expand_field(name) bool name;
    MOS_ALL_DEBUG_MODULES(_expand_field)
#undef _expand_field
} mos_debug_info;

#define mos_debug_enabled(name) (mos_debug_info.name)
#else
#define mos_debug_enabled(name) MOS_DEBUG_FEATURE(name)
#endif
