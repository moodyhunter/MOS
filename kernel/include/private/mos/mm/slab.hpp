// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <stddef.h>

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

typedef struct _slab
{
    as_linked_list;
    spinlock_t lock;
    ptr_t first_free;
    size_t ent_size;
    const char *name;
    size_t nobjs;
} slab_t;

slab_t *kmemcache_create(const char *name, size_t ent_size);
void *kmemcache_alloc(slab_t *slab);
