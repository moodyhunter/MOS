// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/memory.h"
#include "libuserspace++.hpp"
#include "mos/types.h"

void *operator new(size_t sz) noexcept
{
    return malloc(sz);
}

void *operator new[](size_t sz) noexcept
{
    return malloc(sz);
}

void operator delete(void *ptr) noexcept
{
    free(ptr);
}

void operator delete[](void *ptr) noexcept
{
    free(ptr);
}

void operator delete(void *, long unsigned int) noexcept
{
    // Do nothing
}

void operator delete[](void *, long unsigned int) noexcept
{
    // Do nothing
}

namespace mos
{
    const output_stream cout(stdout);
    const output_stream cerr(stderr);
    const endl_t endl;

    const output_stream &operator<<(const output_stream &os, const mos::endl_t &)
    {
        dprintf(os.fd, "\r\n");
        return os;
    }

    const output_stream &operator<<(const output_stream &s, const void *ptr)
    {
        dprintf(s.fd, "%p", ptr);
        return s;
    }

    const output_stream &operator<<(const output_stream &os, const char c)
    {
        dprintf(os.fd, "%c", c);
        return os;
    }

    const output_stream &operator<<(const output_stream &s, const char *str)
    {
        dprintf(s.fd, "%s", str);
        return s;
    }

    const output_stream &operator<<(const output_stream &s, signed int val)
    {
        dprintf(s.fd, "%d", val);
        return s;
    }

    const output_stream &operator<<(const output_stream &s, unsigned int val)
    {
        dprintf(s.fd, "%u", val);
        return s;
    }

    const output_stream &operator<<(const output_stream &s, signed long val)
    {
        dprintf(s.fd, "%ld", val);
        return s;
    }

    const output_stream &operator<<(const output_stream &s, unsigned long val)
    {
        dprintf(s.fd, "%lu", val);
        return s;
    }

    const output_stream &operator<<(const output_stream &s, signed long long val)
    {
        dprintf(s.fd, "%lld", val);
        return s;
    }

    const output_stream &operator<<(const output_stream &s, unsigned long long val)
    {
        dprintf(s.fd, "%llu", val);
        return s;
    }
} // namespace mos
