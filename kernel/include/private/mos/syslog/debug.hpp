// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform_defs.hpp"

#include <mos/mos_global.h>

#ifndef MOS_PLATFORM_DEBUG_MODULES
#define MOS_PLATFORM_DEBUG_MODULES(X)
#endif

// clang-format off
// ! Keep this list consistent with arch/generic/Kconfig.debug
#define MOS_GENERIC_PLATFORM_DEBUG_MODULES(X) \
    X(limine)

// ! Keep this list consistent with kernel/Kconfig.debug
#define MOS_ALL_DEBUG_MODULES(X)    \
    MOS_PLATFORM_DEBUG_MODULES(X)   \
    MOS_GENERIC_PLATFORM_DEBUG_MODULES(X) \
    X(cpio)         \
    X(dcache)       \
    X(dcache_ref)   \
    X(dma)          \
    X(elf)          \
    X(futex)        \
    X(io)           \
    X(ipc)          \
    X(ipi)          \
    X(naive_sched)  \
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

typedef struct _debug_info_entry
{
    const u32 id;
    const char *name;
    bool enabled;
} debug_info_entry;

extern struct mos_debug_info_entry
{
#define _expand_field(name) debug_info_entry name;
    MOS_ALL_DEBUG_MODULES(_expand_field)
#undef _expand_field
} mos_debug_info;

#define _mos_debug_enum(name) name,
enum DebugFeature
{
    MOS_ALL_DEBUG_MODULES(_mos_debug_enum) //
    _none,
};
#undef _mos_debug_enum

/// debug enum to mos_debug_info_entry mapping
#define _mos_debug_info_entry(name) [name] = &mos_debug_info.name,
static inline constexpr debug_info_entry *const mos_debug_info_map[] = {
    MOS_ALL_DEBUG_MODULES(_mos_debug_info_entry) //
        [_none] = nullptr,
};
#undef _mos_debug_info_entry

#define mos_debug_enabled(name)     (mos_debug_info.name.enabled)
#define mos_debug_enabled_ptr(name) (&mos_debug_info.name.enabled)
#else
#define mos_debug_enabled(name)     MOS_DEBUG_FEATURE(name)
#define mos_debug_enabled_ptr(name) NULL
#endif
