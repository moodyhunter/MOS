// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/hashmap_common.h"

#include "lib/hash.h"
#include "lib/string.h"

hash_t __pure hashmap_hash_string(const void *key)
{
    return string_hash((const char *) key, strlen((const char *) key));
}

int __pure hashmap_compare_string(const void *key1, const void *key2)
{
    return strcmp((const char *) key1, (const char *) key2) == 0;
}
