// SPDX-License-Identifier: GPL-3.0-or-later
// Common hash functions and compare functions for hashmap.

#pragma once

#include "mos/types.h"

#define hashmap_common_type_init(map, cap, type) hashmap_init(map, cap, hashmap_functions_for(type))
#define hashmap_functions_for(type)              hashmap_hash_##type, hashmap_compare_##type

hash_t __pure hashmap_hash_string(const void *key);
int __pure hashmap_compare_string(const void *key1, const void *key2);
