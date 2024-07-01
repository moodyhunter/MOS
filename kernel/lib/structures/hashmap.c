// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/lib/sync/spinlock.h"
#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"

#include <mos/lib/structures/hashmap.h>
#include <mos/moslib_global.h>
#include <mos_stdlib.h>
#include <mos_string.h>

#define HASHMAP_MAGIC MOS_FOURCC('H', 'M', 'a', 'p')

typedef struct hashmap_entry
{
    ptr_t key;
    void *value;
    hashmap_entry_t *next;
} hashmap_entry_t;

slab_t *hashmap_entry_slab = NULL;
SLAB_AUTOINIT("hashmap_entry", hashmap_entry_slab, hashmap_entry_t);

void hashmap_init(hashmap_t *map, size_t capacity, hashmap_hash_t hash_func, hashmap_key_compare_t compare_func)
{
    MOS_LIB_ASSERT(map);
    if (map->magic == HASHMAP_MAGIC)
    {
        mos_panic("hashmap_init: hashmap %p is already initialized", (void *) map);
        return;
    }
    memzero(map, sizeof(hashmap_t));
    map->magic = HASHMAP_MAGIC;
    map->entries = kcalloc(capacity, sizeof(hashmap_entry_t *));
    map->capacity = capacity;
    map->size = 0;
    map->hash_func = hash_func;
    map->key_compare_func = compare_func;
}

/**
 * @brief Deinitialize a hashmap.
 * @pre The hashmap must be initialized.
 * @pre The hashmap should be empty, otherwise the entries will be leaked.
 * @warning This function does not free the hashmap itself, nor does it free the keys or values, but only the internal data structures.
 *
 * @param map The hashmap to deinitialize.
 */
void hashmap_deinit(hashmap_t *map)
{
    MOS_LIB_ASSERT_X(map && map->magic == HASHMAP_MAGIC, "hashmap_put: hashmap %p is not initialized", (void *) map);
    spinlock_acquire(&map->lock);
    for (size_t i = 0; i < map->capacity; i++)
    {
        hashmap_entry_t *entry = map->entries[i];
        while (entry != NULL)
        {
            hashmap_entry_t *next = entry->next;
            kfree(entry);
            entry = next;
        }
    }
    kfree(map->entries);
    spinlock_release(&map->lock);
}

void *hashmap_put(hashmap_t *map, uintn key, void *value)
{
    MOS_LIB_ASSERT_X(map && map->magic == HASHMAP_MAGIC, "hashmap_put: hashmap %p is not initialized", (void *) map);
    spinlock_acquire(&map->lock);
    size_t index = map->hash_func(key).hash % map->capacity;
    hashmap_entry_t *entry = map->entries[index];
    while (entry != NULL)
    {
        if (map->key_compare_func(entry->key, key))
        {
            // key already exists, replace value
            void *old_value = entry->value;
            entry->value = value;
            spinlock_release(&map->lock);
            return old_value;
        }
        entry = entry->next;
    }
    entry = kmalloc(hashmap_entry_slab);
    entry->key = key;
    entry->value = value;
    entry->next = map->entries[index];
    map->entries[index] = entry;
    map->size++;
    spinlock_release(&map->lock);
    return NULL;
}

void *hashmap_get(hashmap_t *map, uintn key)
{
    MOS_LIB_ASSERT_X(map && map->magic == HASHMAP_MAGIC, "hashmap_put: hashmap %p is not initialized", (void *) map);
    spinlock_acquire(&map->lock);
    size_t index = map->hash_func(key).hash % map->capacity;
    hashmap_entry_t *entry = map->entries[index];
    while (entry != NULL)
    {
        if (map->key_compare_func(entry->key, key))
        {
            void *value = entry->value;
            spinlock_release(&map->lock);
            return value;
        }
        entry = entry->next;
    }

    spinlock_release(&map->lock);
    return NULL;
}

void *hashmap_remove(hashmap_t *map, uintn key)
{
    MOS_LIB_ASSERT_X(map && map->magic == HASHMAP_MAGIC, "hashmap_put: hashmap %p is not initialized", (void *) map);
    spinlock_acquire(&map->lock);
    size_t index = map->hash_func(key).hash % map->capacity;
    hashmap_entry_t *entry = map->entries[index];
    hashmap_entry_t *prev = NULL;
    while (entry != NULL)
    {
        if (map->key_compare_func(entry->key, key))
        {
            if (prev == NULL)
                map->entries[index] = entry->next;
            else
                prev->next = entry->next;
            void *value = entry->value;
            kfree(entry);
            map->size--;
            spinlock_release(&map->lock);
            return value;
        }
        prev = entry;
        entry = entry->next;
    }

    spinlock_release(&map->lock);
    return NULL;
}

void hashmap_foreach(hashmap_t *map, hashmap_foreach_func_t func, void *data)
{
    MOS_LIB_ASSERT_X(map && map->magic == HASHMAP_MAGIC, "hashmap_put: hashmap %p is not initialized", (void *) map);
    for (size_t i = 0; i < map->capacity; i++)
    {
        hashmap_entry_t *entry = map->entries[i];
        while (entry != NULL)
        {
            if (!func(entry->key, entry->value, data))
                return;
            entry = entry->next;
        }
    }
}
