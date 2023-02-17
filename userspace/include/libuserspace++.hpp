// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/sync/mutex.h"
#include "mos/types.h"

#define MOS_DISABLE_COPY_AND_MOVE(classname)                                                                                                                             \
    classname(const classname &) = delete;                                                                                                                               \
    classname &operator=(const classname &) = delete;                                                                                                                    \
    classname(classname &&) = delete;                                                                                                                                    \
    classname &operator=(classname &&) = delete;

namespace mos
{
    class output_stream
    {
      public:
        int fd;
        explicit output_stream(int fd) : fd(fd){};
        ~output_stream() = default; // do we close the fd?

      private:
        MOS_DISABLE_COPY_AND_MOVE(output_stream)
    };

    extern output_stream cout;
    extern output_stream cerr;

    extern output_stream &endl(output_stream &os);

    inline output_stream &operator<<(output_stream &os, output_stream &(*manip)(output_stream &os))
    {
        return manip(os);
    }

    output_stream &operator<<(output_stream &s, const void *ptr);
    output_stream &operator<<(output_stream &os, const char c);
    output_stream &operator<<(output_stream &s, const char *str);
    output_stream &operator<<(output_stream &s, signed int val);
    output_stream &operator<<(output_stream &s, unsigned int val);
    output_stream &operator<<(output_stream &s, signed long val);
    output_stream &operator<<(output_stream &s, unsigned long val);
    output_stream &operator<<(output_stream &s, signed long long val);
    output_stream &operator<<(output_stream &s, unsigned long long val);

    class mutex
    {
      public:
        explicit mutex()
        {
            mutex_init(&this->value);
        }

        ~mutex()
        {
            mutex_release(&this->value);
        }

        void lock()
        {
            mutex_acquire(&this->value);
        }

        void unlock()
        {
            mutex_release(&this->value);
        }

      private:
        MOS_DISABLE_COPY_AND_MOVE(mutex)

      private:
        mutex_t value;
    };

    class lock_guard
    {
      public:
        explicit lock_guard(mutex &m) : m_mutex(m)
        {
            m_mutex.lock();
        }

        ~lock_guard()
        {
            m_mutex.unlock();
        }

      private:
        MOS_DISABLE_COPY_AND_MOVE(lock_guard)

      private:
        mutex &m_mutex;
    };
} // namespace mos
