// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstddef>

namespace mos
{
    struct RCCore
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

    struct RefCounted
    {
      protected:
        explicit RefCounted(RCCore *rc_) : rc(rc_)
        {
            rc->Ref();
        }

        RefCounted(const RefCounted &a) : rc(a.rc)
        {
            rc->Ref();
        }

        RefCounted(RefCounted &&a) : rc(a.rc)
        {
            a.rc = nullptr;
        }

        ~RefCounted()
        {
            if (rc)
            {
                rc->Unref();
            }
        }

        RefCounted operator=(const RefCounted &a) = delete;
        RefCounted operator=(RefCounted &&a) = delete;

      protected:
        size_t GetRef() const
        {
            return rc->n;
        }

      private:
        RCCore *rc = nullptr;
    };
} // namespace mos
