// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/hashmap.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

typedef struct hashmap_entry
{
    const void *key;
    void *value;
    hashmap_entry_t *next;
} hashmap_entry_t;

void hashmap_init(hashmap_t *map, size_t capacity, hashmap_hash_t hash_func, hashmap_key_compare_t compare_func)
{
    MOS_ASSERT(map);
    if (map->magic == HASHMAP_MAGIC)
    {
        mos_panic("hashmap_init: hashmap %p is already initialized", (void *) map);
        return;
    }
    map->magic = HASHMAP_MAGIC;
    map->entries = kmalloc(sizeof(hashmap_entry_t *) * capacity);
    memset(map->entries, 0, sizeof(hashmap_entry_t *) * capacity);
    map->capacity = capacity;
    map->size = 0;
    map->hash_func = hash_func;
    map->key_compare_func = compare_func;
}

void hashmap_deinit(hashmap_t *map)
{
    MOS_ASSERT_X(map && map->magic == HASHMAP_MAGIC, "hashmap_put: hashmap %p is not initialized", (void *) map);
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
}

void *hashmap_put(hashmap_t *map, const void *key, void *value)
{
    MOS_ASSERT_X(map && map->magic == HASHMAP_MAGIC, "hashmap_put: hashmap %p is not initialized", (void *) map);
    size_t index = map->hash_func(key).hash % map->capacity;
    hashmap_entry_t *entry = map->entries[index];
    while (entry != NULL)
    {
        if (map->key_compare_func(entry->key, key))
        {
            // key already exists, replace value
            void *old_value = entry->value;
            entry->value = value;
            return old_value;
        }
        entry = entry->next;
    }
    entry = kmalloc(sizeof(hashmap_entry_t));
    entry->key = key;
    entry->value = value;
    entry->next = map->entries[index];
    map->entries[index] = entry;
    map->size++;
    return NULL;
}

void *hashmap_get(const hashmap_t *map, const void *key)
{
    MOS_ASSERT_X(map && map->magic == HASHMAP_MAGIC, "hashmap_put: hashmap %p is not initialized", (void *) map);
    size_t index = map->hash_func(key).hash % map->capacity;
    hashmap_entry_t *entry = map->entries[index];
    while (entry != NULL)
    {
        if (map->key_compare_func(entry->key, key))
        {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

void *hashmap_remove(hashmap_t *map, const void *key)
{
    MOS_ASSERT_X(map && map->magic == HASHMAP_MAGIC, "hashmap_put: hashmap %p is not initialized", (void *) map);
    size_t index = map->hash_func(key).hash % map->capacity;
    hashmap_entry_t *entry = map->entries[index];
    hashmap_entry_t *prev = NULL;
    while (entry != NULL)
    {
        if (map->key_compare_func(entry->key, key))
        {
            if (prev == NULL)
            {
                map->entries[index] = entry->next;
            }
            else
            {
                prev->next = entry->next;
            }
            void *value = entry->value;
            kfree(entry);
            map->size--;
            return value;
        }
        prev = entry;
        entry = entry->next;
    }
    return NULL;
}

void hashmap_foreach(hashmap_t *map, hashmap_foreach_func_t func)
{
    MOS_ASSERT_X(map && map->magic == HASHMAP_MAGIC, "hashmap_put: hashmap %p is not initialized", (void *) map);
    for (size_t i = 0; i < map->capacity; i++)
    {
        hashmap_entry_t *entry = map->entries[i];
        while (entry != NULL)
        {
            if (!func(entry->key, entry->value))
                return;
            entry = entry->next;
        }
    }
}
