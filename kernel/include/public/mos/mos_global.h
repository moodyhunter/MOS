// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifdef __MOS_KERNEL__
#include <abi-bits/errno.h>
#endif
#include <mos/compiler.h>
#include <stddef.h>
#include <stdnoreturn.h>

#if !defined(noreturn)
#define noreturn __attribute__((noreturn))
#endif

#ifdef __cplusplus
#define MOS_STATIC_ASSERT static_assert
#else
#define MOS_STATIC_ASSERT _Static_assert
#endif

/**
 * @brief Helper macro to make sure the offset of a field in a struct is the same as another struct.
 */
#define MOS_ASSERT_OFFSET(src_t, src_f, dst_t, dst_f, msg) MOS_STATIC_ASSERT(offsetof(src_t, src_f) == offsetof(dst_t, dst_f), msg)

#if MOS_COMPILER_GCC
#define __mos_copy_attr(target) __attribute__((__copy__(target)))
#else
#define __mos_copy_attr(target)
#endif

// clang-format off
#if defined(__MOS_KERNEL__) && defined(__cplusplus)
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
// clang-format on

#define __alias(target, func) extern __typeof__(target) func __attribute__((__alias__(#target))) __mos_copy_attr(target)

#define __aligned(x)    __attribute__((__aligned__(x)))
#define __cold          __attribute__((__cold__))
#define __malloc        __attribute__((__malloc__))
#define __packed        __attribute__((__packed__))
#define __printf(a, b)  __attribute__((__format__(__printf__, a, b)))
#define __pure          __attribute__((__pure__))
#define __section(S)    __attribute__((__section__(S)))
#define __maybe_unused  __attribute__((__unused__))
#define __used          __attribute__((__used__))
#define __nodiscard     __attribute__((__warn_unused_result__))
#define __no_instrument __attribute__((__no_instrument_function__))

#define asmlinkage    __attribute__((sysv_abi))
#define should_inline __maybe_unused static inline

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define __types_compatible(a, b) __builtin_types_compatible_p(__typeof(a), __typeof(b))
#define do_container_of(ptr, type, member)                                                                                                                               \
    __extension__({                                                                                                                                                      \
        void *real_ptr = (void *) (ptr);                                                                                                                                 \
        _Static_assert(__types_compatible(*(ptr), ((type *) 0)->member) | __types_compatible(*(ptr), void), "type mismatch: (" #type ") vs (" #ptr "->" #member ")");    \
        ((type *) (real_ptr - offsetof(type, member)));                                                                                                                  \
    })

#define container_of(ptr, type, member)                                                                                                                                  \
    _Generic(ptr, const __typeof(*(ptr)) *: ((const type *) do_container_of(ptr, type, member)), default: ((type *) do_container_of(ptr, type, member)))

#define cast_to(value, valtype, desttype) _Generic((value), valtype: (desttype) (value), const valtype: (const desttype)(value))

#define is_aligned(ptr, alignment) (((ptr_t) ptr & (alignment - 1)) == 0)

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

#define ALIGN_UP(addr, size)     (((addr) + (size - 1)) & ~(size - 1))
#define ALIGN_DOWN(addr, size)   ((addr) & ~(size - 1))
#define ALIGN_UP_TO_PAGE(addr)   ALIGN_UP(addr, MOS_PAGE_SIZE)
#define ALIGN_DOWN_TO_PAGE(addr) ALIGN_DOWN(addr, MOS_PAGE_SIZE)

#define MOS_IN_RANGE(addr, start, end)       ((addr) >= (start) && (addr) < (end))
#define SUBSET_RANGE(addr, size, start, end) (MOS_IN_RANGE(addr, start, end) && MOS_IN_RANGE(addr + size, start, end))

#define MOS_FOURCC(a, b, c, d) ((u32) (a) | ((u32) (b) << 8) | ((u32) (c) << 16) | ((u32) (d) << 24))
#define MOS_ARRAY_SIZE(x)      (sizeof(x) / sizeof(x[0]))

#define MOS_MAX_VADDR ((ptr_t) ~0)

#define MOS_SYSCALL_INTR             0x88
#define BIOS_VADDR(paddr)            (MOS_HWMEM_VADDR | ((ptr_t) (paddr)))
#define BIOS_VADDR_TYPE(paddr, type) ((type) BIOS_VADDR((paddr)))

#define READ_ONCE(x) (*(volatile typeof(x) *) &(x))

// clang-format off
#define KB * 1024
#define MB * 1024 KB
#define GB * (u64) 1024 MB
#define TB * (u64) 1024 GB
#define statement_expr(type, ...) __extension__({ type retval; __VA_ARGS__; retval; })
// clang-format on

#define __NO_OP(...)

#define BIT(x) (1ull << (x))

#ifdef __cplusplus
#define MOSAPI extern "C"
#else
#define MOSAPI extern
#endif

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

#define MOS_PUT_IN_SECTION(_section, _struct, _var, ...) static const _struct _var __used __section(_section) = __VA_ARGS__
#define IS_ERR_VALUE(x)                                  unlikely((unsigned long) (void *) (x) >= (unsigned long) -4095)

__nodiscard should_inline void *ERR_PTR(long error)
{
    return (void *) error;
}

__nodiscard should_inline long PTR_ERR(const void *ptr)
{
    return (long) ptr;
}

__nodiscard should_inline void *ERR(const void *ptr)
{
    return (void *) ptr;
}

__nodiscard should_inline bool IS_ERR(const void *ptr)
{
    return IS_ERR_VALUE((unsigned long) ptr);
}

__attribute__((__deprecated__("reconsider if a NULL check is really required"))) __nodiscard should_inline bool IS_ERR_OR_NULL(const void *ptr)
{
    return unlikely(!ptr) || IS_ERR_VALUE((unsigned long) ptr);
}

#define MOS_STUB_IMPL(...)                                                                                                                                               \
    MOS_WARNING_PUSH                                                                                                                                                     \
    MOS_WARNING_DISABLE("-Wunused-parameter")                                                                                                                            \
    __VA_ARGS__                                                                                                                                                          \
    {                                                                                                                                                                    \
        MOS_UNREACHABLE_X("unimplemented: file %s, line %d", __FILE__, __LINE__);                                                                                        \
    }                                                                                                                                                                    \
    MOS_WARNING_POP

#ifdef __cplusplus
// enum operators are not supported in C++ implicitly
// clang-format off
#define MOS_ENUM_OPERATORS(_enum) \
    constexpr inline _enum operator|(_enum a, _enum b) { return static_cast<_enum>(static_cast<int>(a) | static_cast<int>(b)); } \
    constexpr inline _enum operator&(_enum a, _enum b) { return static_cast<_enum>(static_cast<int>(a) & static_cast<int>(b)); } \
    constexpr inline _enum operator~(_enum a) { return static_cast<_enum>(~static_cast<int>(a)); } \
    constexpr inline _enum &operator|=(_enum &a, _enum b) { return a = a | b; } \
    constexpr inline _enum &operator&=(_enum &a, _enum b) { return a = a & b; }
// clang-format on
#else
#define MOS_ENUM_OPERATORS(_enum)
#endif
