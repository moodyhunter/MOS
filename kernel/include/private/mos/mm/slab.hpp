// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/string_view.hpp>
#include <mos/syslog/printk.hpp>
#include <stddef.h>

/**
 * @brief initialise the slab allocator
 *
 */
void slab_init();

/**
 * @brief Allocate a block of memory from the slab allocator.
 *
 * @param size
 * @return void*
 */
void *slab_alloc(size_t size);

/**
 * @brief Allocate a block of memory from the slab allocator and zero it.
 *
 * @param nmemb
 * @param size
 * @return void*
 */
void *slab_calloc(size_t nmemb, size_t size);

/**
 * @brief Reallocate a block of memory from the slab allocator.
 *
 * @param addr
 * @param size
 * @return void*
 */
void *slab_realloc(void *addr, size_t size);

/**
 * @brief Free a block of memory from the slab allocator.
 *
 * @param addr
 */
void slab_free(const void *addr);

struct slab_t
{
    as_linked_list;
    spinlock_t lock = SPINLOCK_INIT;
    ptr_t first_free = 0;
    size_t ent_size = 0;
    const char *name = "<unnamed>";
    size_t nobjs = 0;
};

void slab_register(slab_t *slab);
void *kmemcache_alloc(slab_t *slab);
void kmemcache_free(slab_t *slab, const void *addr);

namespace mos
{
    template<typename T>
    struct Slab : public slab_t
    {
        constexpr Slab(mos::string_view name = T::type_name, size_t size = sizeof(T))
        {
            this->name = name.data();
            this->ent_size = size;
        }

        ~Slab()
        {
            pr_emerg("slab: freeing slab for '%s'", this->name);
        }

        template<typename... Args>
        T *create(Args &&...args)
        {
            if (!registered)
                slab_register(this), registered = true;

            const auto ptr = kmemcache_alloc(this);
            new (ptr) T(std::forward<Args>(args)...);
            return static_cast<T *>(ptr);
        }

        size_t size()
        {
            return ent_size;
        }

      private:
        bool registered = false;
    };
} // namespace mos
