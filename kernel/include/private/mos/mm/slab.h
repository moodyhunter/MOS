// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <stddef.h>

/**
 * @brief Initialize the slab allocator.
 *
 */
void slab_init(void);

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

typedef struct
{
    as_linked_list;
    spinlock_t lock;
    ptr_t first_free;
    size_t ent_size;
    const char *name;
} slab_t;

slab_t *kmemcache_create(const char *name, size_t ent_size);
void *kmemcache_alloc(slab_t *slab);
void kmemcache_free(slab_t *slab, const void *addr);
