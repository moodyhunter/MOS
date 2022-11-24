// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/types.h"


typedef hash_t (*hashmap_hash_t)(const void *key);
typedef int (*hashmap_key_compare_t)(const void *key1, const void *key2);
typedef bool (*hashmap_foreach_func_t)(const void *key, void *value);

typedef struct hashmap_entry hashmap_entry_t;

typedef struct _hashmap
{
    s32 magic;
    hashmap_entry_t **entries;
    size_t capacity;
    size_t size;
    hashmap_hash_t hash_func;
    hashmap_key_compare_t key_compare_func;
} hashmap_t;

// ! These init/deinit functions does not allocate/deallocate the hashmap itself.
void hashmap_init(hashmap_t *map, size_t capacity, hashmap_hash_t hash_func, hashmap_key_compare_t compare_func);
void hashmap_deinit(hashmap_t *map);

void *hashmap_put(hashmap_t *map, const void *key, void *value);
void *hashmap_get(const hashmap_t *map, const void *key);
void *hashmap_remove(hashmap_t *map, const void *key);

void hashmap_foreach(hashmap_t *map, hashmap_foreach_func_t func);
