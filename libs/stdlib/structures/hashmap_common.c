// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/lib/structures/hashmap_common.h>
#include <mos/types.h>
#include <string.h>

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

hash_t __pure hashmap_hash_string(uintn key)
{
    return string_hash((const char *) key, strlen((const char *) key));
}

int __pure hashmap_compare_string(uintn key1, uintn key2)
{
    return strcmp((const char *) key1, (const char *) key2) == 0;
}

int __pure hashmap_simple_key_compare(uintn key1, uintn key2)
{
    return key1 == key2;
}

hash_t hashmap_identity_hash(uintn key)
{
    return (hash_t){ .hash = key };
}
