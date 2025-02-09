// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/moslib_global.hpp>
#include <mos/types.hpp>

/**
 * @defgroup libs_hashmap_commonfuncs libs.HashMap.Common
 * @ingroup libs_hashmap
 * @brief Common hash and comparison functions for hashmaps.
 * @{
 */
#define hashmap_common_type_init(map, cap, type) hashmap_init(map, cap, hashmap_hash_##type, hashmap_compare_##type)

MOSAPI hash_t __pure hashmap_hash_string(uintn key);
MOSAPI int __pure hashmap_compare_string(uintn key1, uintn key2);

MOSAPI int __pure hashmap_simple_key_compare(uintn key1, uintn key2);

MOSAPI hash_t __pure hashmap_identity_hash(uintn key);
/** @} */
