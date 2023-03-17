// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/compiler.h"

#include <stdnoreturn.h>

#define __aligned(x)    __attribute__((__aligned__(x)))
#define __cold          __attribute__((__cold__))
#define __malloc        __attribute__((__malloc__))
#define __packed        __attribute__((__packed__))
#define __printf(a, b)  __attribute__((__format__(__printf__, a, b)))
#define __pure          __attribute__((__pure__))
#define __section(S)    __attribute__((__section__(S)))
#define __maybe_unused  __attribute__((__unused__))
#define __used          __attribute__((__used__))
#define __weak_alias(x) __attribute__((weak, alias(x)))
#define __weakref(x)    __attribute__((weakref(x)))
#define __nodiscard     __attribute__((__warn_unused_result__))

#define asmlinkage    __attribute__((regparm(0)))
#define should_inline __maybe_unused static inline
#define always_inline should_inline __attribute__((__always_inline__))

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define to_union(u) __extension__(u)

#define container_of(ptr, type, member) ((type *) ((char *) (ptr) - (offsetof(type, member))))

#define add_const(x) (*(const __typeof__(x) *) (&(x)))

#define is_const_instance(ptr, ptrtype, tval, fval)    _Generic((ptr), const ptrtype * : tval, ptrtype * : fval)
#define select_const_cast(ptr, ptrtype, obj, objtype)  is_const_instance(ptr, ptrtype, (const objtype)(obj), (objtype) (obj))
#define const_container_of(ptr, ptrtype, type, member) (select_const_cast(ptr, ptrtype, (container_of(ptr, type, member)), type *))

#define GET_BIT(x, n)               (((x) >> (n)) & 1)
#define MASK_BITS(value, width)     ((value) & ((1 << (width)) - 1))
#define SET_BITS(bit, width, value) (MASK_BITS(value, width) << (bit))

#define MOS_STRINGIFY2(x) #x
#define MOS_STRINGIFY(x)  MOS_STRINGIFY2(x)

#define MOS_UNUSED(x) (void) (x)

#define MOS_CONCAT_INNER(a, b) a##b
#define MOS_CONCAT(a, b)       MOS_CONCAT_INNER(a, b)

#define MOS_WARNING_PUSH          MOS_PRAGMA(diagnostic push)
#define MOS_WARNING_POP           MOS_PRAGMA(diagnostic pop)
#define MOS_WARNING_DISABLE(text) MOS_PRAGMA(diagnostic ignored text)

#define ALIGN_UP(addr, size)     (((addr) + size - 1) & ~(size - 1))
#define ALIGN_DOWN(addr, size)   ((addr) & ~(size - 1))
#define ALIGN_UP_TO_PAGE(addr)   ALIGN_UP(addr, MOS_PAGE_SIZE)
#define ALIGN_DOWN_TO_PAGE(addr) ALIGN_DOWN(addr, MOS_PAGE_SIZE)

#define IN_RANGE(addr, start, end)           ((addr) >= (start) && (addr) < (end))
#define SUBSET_RANGE(addr, size, start, end) (IN_RANGE(addr, start, end) && IN_RANGE(addr + size, start, end))

#define MOS_FOURCC(a, b, c, d) ((u32) (a) | ((u32) (b) << 8) | ((u32) (c) << 16) | ((u32) (d) << 24))
#define MOS_ARRAY_SIZE(x)      (sizeof(x) / sizeof(x[0]))

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

// If the feature is enabled, the expression will be 1, otherwise -1.
// If the given feature is not defined, the expression will be 0, which throws a division by zero error.
#define MOS_CONFIG(feat) (1 / feat == 1)

#define MOS_DEBUG_FEATURE(feat) MOS_CONFIG(MOS_CONCAT(MOS_DEBUG_, feat))

/**
 * @brief Returns true for the first call, false for all subsequent calls.
 */
#define once()                                                                                                                                                           \
    __extension__({                                                                                                                                                      \
        static bool __seen = false;                                                                                                                                      \
        bool ret = false;                                                                                                                                                \
        if (__seen)                                                                                                                                                      \
            ret = false;                                                                                                                                                 \
        else                                                                                                                                                             \
            __seen = true, ret = true;                                                                                                                                   \
        ret;                                                                                                                                                             \
    })
