// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/hashmap_common.h"

#include "lib/string.h"

static hash_t __pure string_hash(const char *s, const int n)
{
    const int p = 31, m = 1e9 + 7;
    hash_t h = { 0 };
    long p_pow = 1;
    for (int i = 0; i < n; i++)
    {
        h.hash = (h.hash + (s[i] - 'a' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }
    return h;
}

hash_t __pure hashmap_hash_string(const void *key)
{
    return string_hash((const char *) key, strlen((const char *) key));
}

int __pure hashmap_compare_string(const void *key1, const void *key2)
{
    return strcmp((const char *) key1, (const char *) key2) == 0;
}

int __pure hashmap_simple_key_compare(const void *key1, const void *key2)
{
    return key1 == key2;
}
