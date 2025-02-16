// SPDX-License-Identifier: GPL-3.0-or-later

#include <cxxabi.h>
#include <mos/assert.hpp>
#include <mos/string.hpp>
#include <mos_stdlib.hpp>

void *__dso_handle = (void *) 0xcdcdcdcdcdcdcdcd; // this pointer should never get dereferenced

void operator delete(void *)
{
    mos_panic("unused operator delete called");
}

void operator delete(void *ptr, size_t) noexcept
{
    do_kfree(ptr);
}

extern "C" int __cxa_atexit(void (*destructor)(void *), void *arg, void *dso)
{
    MOS_UNUSED(destructor);
    MOS_UNUSED(arg);
    MOS_UNUSED(dso);
    return 0;
}

extern "C" void abort()
{
    mos_panic("Aborted");
}

// static scoped variable constructor support

extern "C" int abi::__cxa_guard_acquire(abi::__guard *g)
{
    // TODO: stub functions, implement 64-bit mutexes/futex-word
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    long val;
    __atomic_load(g, &val, __ATOMIC_RELAXED);
    return !val;
}

// this function is called when a constructor finishes
extern "C" void abi::__cxa_guard_release(abi::__guard *g)
{
    long zero = 0;
    __atomic_store(g, &zero, __ATOMIC_RELEASE);
    __atomic_thread_fence(__ATOMIC_RELEASE);
}

// this function is called when a constructor throws an exception
extern "C" void abi::__cxa_guard_abort(abi::__guard *)
{
}
