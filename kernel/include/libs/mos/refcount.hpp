// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstddef>
#include <mos/allocator.hpp>
#include <mos/type_utils.hpp>

namespace mos
{
    struct RefCounted
    {
      protected:
        RefCounted() : rc(mos::create<RC>())
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
                if (rc->n == 0)
                    delete rc;
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
        struct RC : mos::NamedType<"RC">
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

        RC *rc = nullptr;
    };
} // namespace mos
