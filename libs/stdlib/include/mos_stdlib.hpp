// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/tasks/signal_types.h>
#include <mos/types.h>
#include <stddef.h>

#if defined(__MOS_KERNEL__) && defined(__cplusplus)
#include <mos/allocator.hpp>
#include <type_traits>
#endif

/**
 * @defgroup libs_stdlib libs.Stdlib
 * @ingroup libs
 * @brief Some standard library functions.
 * @{
 */

MOSAPI unsigned char tolower(unsigned char c);
MOSAPI s32 abs(s32 x);
MOSAPI long labs(long x);
MOSAPI s64 llabs(s64 x);
MOSAPI s32 atoi(const char *nptr);

MOSAPI unsigned long strtoul(const char *nptr, char **endptr, int base);
MOSAPI s64 strtoll(const char *str, char **endptr, int base);
MOSAPI s64 strntoll(const char *str, char **endptr, int base, size_t n);

MOSAPI void format_size(char *buf, size_t buf_size, u64 size);
MOSAPI char *string_trim(char *in);

// clang-format off
#define MIN(a, b) __extension__ ({ const auto _a = (a); __auto_type _b = (b); _a < _b ? _a : _b; })
#define MAX(a, b) __extension__ ({ const auto _a = (a); __auto_type _b = (b); _a > _b ? _a : _b; })
// clang-format on

#ifdef __MOS_KERNEL__
#ifdef __cplusplus

template<typename T>
concept KMallocCapable = std::is_fundamental_v<std::remove_pointer_t<T>> || std::is_pointer_v<T>;

template<typename T, typename... Args>
requires(KMallocCapable<T>) T *kmalloc(Args &&...args)
{
    void *ptr = do_kmalloc(sizeof(T));
    if (ptr == nullptr)
        return nullptr;

    return new (ptr) T(std::forward<Args>(args)...);
}

template<typename T>
requires(KMallocCapable<T>) T *kcalloc(size_t n_members)
{
    return (T *) do_kcalloc(n_members, sizeof(T));
}

template<typename T>
requires(KMallocCapable<T>) T *krealloc(T *ptr, size_t size)
{
    return (T *) do_krealloc((void *) ptr, size);
}

template<typename T>
requires(KMallocCapable<T>) void kfree(T *ptr)
{
    do_kfree((const void *) ptr);
}

#endif

#ifdef __IN_MOS_LIBS__
MOSAPI void *malloc(size_t size);
MOSAPI void *calloc(size_t nmemb, size_t size);
MOSAPI void *realloc(void *ptr, size_t size);
MOSAPI void free(void *ptr);
#endif
#else // userspace mini stdlib
MOSAPI void exit(int status) __attribute__((noreturn));
MOSAPI int atexit(void (*func)(void));
MOSAPI tid_t start_thread(const char *name, thread_entry_t entry, void *arg);
MOSAPI void abort(void) __attribute__((noreturn));
#endif

/** @} */
