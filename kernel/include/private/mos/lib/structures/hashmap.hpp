// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/allocator.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/moslib_global.hpp>
#include <mos/types.hpp>
#include <mos_stdlib.hpp>

/**
 * @defgroup libs_hashmap libs.HashMap
 * @ingroup libs
 * @brief A simple hashmap.
 * @{
 */

typedef hash_t (*hashmap_hash_t)(const uintn key);                                /// A hashmap hash function prototype.
typedef int (*hashmap_key_compare_t)(const uintn key1, const uintn key2);         /// A hashmap key comparison function prototype.
typedef bool (*hashmap_foreach_func_t)(const uintn key, void *value, void *data); /// A hashmap foreach callback function prototype.

struct hashmap_entry_t;

typedef struct _hashmap
{
    s32 magic;
    hashmap_entry_t **entries;
    size_t capacity;
    size_t size;
    hashmap_hash_t hash_func;
    hashmap_key_compare_t key_compare_func;
    spinlock_t lock;
} hashmap_t;

MOSAPI void hashmap_init(hashmap_t *map, size_t capacity, hashmap_hash_t hash_func, hashmap_key_compare_t compare_func);
MOSAPI void hashmap_deinit(hashmap_t *map);

MOSAPI void *hashmap_put(hashmap_t *map, uintn key, void *value);
MOSAPI void *hashmap_get(hashmap_t *map, uintn key);
MOSAPI void *hashmap_remove(hashmap_t *map, uintn key);

MOSAPI void hashmap_foreach(hashmap_t *map, hashmap_foreach_func_t func, void *data);

/** @} */
