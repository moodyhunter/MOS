// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

namespace mos
{
    // endl manipulator
    struct endl_t
    {
    };

    class output_stream
    {
      public:
        int fd;
        output_stream(int fd) : fd(fd)
        {
        }
        output_stream(const output_stream &) = delete;
        output_stream &operator=(const output_stream &) = delete;
        output_stream(output_stream &&) = delete;
        output_stream &operator=(output_stream &&) = delete;
        ~output_stream() = default;
    };

    extern const ::mos::output_stream cout;
    extern const ::mos::output_stream cerr;
    extern const ::mos::endl_t endl;

    const ::mos::output_stream &operator<<(const ::mos::output_stream &os, const endl_t &endl);
    const ::mos::output_stream &operator<<(const ::mos::output_stream &s, const void *ptr);
    const ::mos::output_stream &operator<<(const ::mos::output_stream &os, const char c);
    const ::mos::output_stream &operator<<(const ::mos::output_stream &s, const char *str);
    const ::mos::output_stream &operator<<(const ::mos::output_stream &s, signed int val);
    const ::mos::output_stream &operator<<(const ::mos::output_stream &s, unsigned int val);
    const ::mos::output_stream &operator<<(const ::mos::output_stream &s, signed long val);
    const ::mos::output_stream &operator<<(const ::mos::output_stream &s, unsigned long val);
    const ::mos::output_stream &operator<<(const ::mos::output_stream &s, signed long long val);
    const ::mos::output_stream &operator<<(const ::mos::output_stream &s, unsigned long long val);

} // namespace mos
