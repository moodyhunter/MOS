// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/lib/sync/spinlock.hpp"

#include <mos/allocator.hpp>
#include <mos/lib/structures/hashmap.hpp>
#include <mos/moslib_global.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

#define HASHMAP_MAGIC MOS_FOURCC('H', 'M', 'a', 'p')

struct hashmap_entry_t : mos::NamedType<"Hashmap.Entry">
{
    ptr_t key;
    void *value;
    hashmap_entry_t *next;
};

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
    map->entries = kcalloc<hashmap_entry_t *>(capacity);
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
    map->magic = 0;
    for (size_t i = 0; i < map->capacity; i++)
    {
        hashmap_entry_t *entry = map->entries[i];
        while (entry != NULL)
        {
            hashmap_entry_t *next = entry->next;
            delete entry;
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
            auto old_value = entry->value;
            entry->value = value;
            spinlock_release(&map->lock);
            return old_value;
        }
        entry = entry->next;
    }
    entry = mos::create<hashmap_entry_t>();
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
            const auto value = entry->value;
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
            delete entry;
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
