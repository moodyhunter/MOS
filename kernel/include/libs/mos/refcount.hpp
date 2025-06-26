// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstddef>

namespace mos
{
    struct RefCount
    {
        void Ref()
        {
            n++;
        }

        void Unref()
        {
            n--;
        }

        size_t GetRef() const
        {
            return n.load();
        }

        bool IsEmpty() const
        {
            return n.load() == 0;
        }

      private:
        std::atomic_size_t n = 0;
    };
    struct _RCCore
    {
        std::atomic_size_t n = 0;
        void Ref()
        {
            n++;
        }

        void Unref()
        {
            n--;
        }
    };

    struct _RefCounted
    {
      protected:
        explicit _RefCounted(_RCCore *rc_) : rc(rc_)
        {
            rc->Ref();
        }

        _RefCounted(const _RefCounted &a) : rc(a.rc)
        {
            rc->Ref();
        }

        _RefCounted(_RefCounted &&a) : rc(a.rc)
        {
            a.rc = nullptr;
        }

        ~_RefCounted()
        {
            if (rc)
            {
                rc->Unref();
            }
        }

        _RefCounted operator=(const _RefCounted &a) = delete;
        _RefCounted operator=(_RefCounted &&a) = delete;

      protected:
        size_t GetRef() const
        {
            return rc->n;
        }

      private:
        _RCCore *rc = nullptr;
    };
} // namespace mos
