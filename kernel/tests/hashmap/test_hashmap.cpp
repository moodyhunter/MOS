// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_engine_impl.h"

#include <mos/lib/structures/hashmap.hpp>
#include <mos/lib/structures/hashmap_common.hpp>
#include <mos/mos_global.h>

#define HASHMAP_MAGIC MOS_FOURCC('H', 'M', 'a', 'p')

MOS_TEST_CASE(hashmap_init_simple_macro)
{
    hashmap_t map = { 0 };
    hashmap_common_type_init(&map, 64, string);
    MOS_TEST_CHECK(map.magic, HASHMAP_MAGIC);
    MOS_TEST_CHECK(map.capacity, 64);
    MOS_TEST_CHECK(map.size, 0);
    MOS_TEST_CHECK(map.hash_func, hashmap_hash_string);
    MOS_TEST_CHECK(map.key_compare_func, hashmap_compare_string);
    MOS_TEST_CHECK(map.entries != NULL, true);
    hashmap_deinit(&map);
}

MOS_TEST_CASE(hashmap_put_single)
{
    hashmap_t map = { 0 };
    hashmap_common_type_init(&map, 135, string);
    MOS_TEST_CHECK(map.magic, HASHMAP_MAGIC);

    MOS_TEST_CHECK(map.capacity, 135);
    MOS_TEST_CHECK(map.size, 0);
    void *old = hashmap_put(&map, (ptr_t) "foo", (void *) "bar");
    MOS_TEST_CHECK(old, NULL);
    MOS_TEST_CHECK(map.capacity, 135);
    MOS_TEST_CHECK(map.size, 1);

    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "foo"), "bar");
    hashmap_deinit(&map);
}

MOS_TEST_CASE(hashmap_get_function)
{
    hashmap_t map = { 0 };
    hashmap_common_type_init(&map, 1, string);
    MOS_TEST_CHECK(map.magic, HASHMAP_MAGIC);
    MOS_TEST_CHECK(map.capacity, 1);
    MOS_TEST_CHECK(map.size, 0);

    hashmap_put(&map, (ptr_t) "foo", (void *) "foo1");
    MOS_TEST_CHECK(map.capacity, 1);
    MOS_TEST_CHECK(map.size, 1);
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "foo"), "foo1");

    hashmap_put(&map, (ptr_t) "bar", (void *) "bar1");
    MOS_TEST_CHECK(map.capacity, 1);
    MOS_TEST_CHECK(map.size, 2);
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "bar"), "bar1");

    hashmap_put(&map, (ptr_t) "bar", (void *) "bar2");
    MOS_TEST_CHECK(map.capacity, 1);
    MOS_TEST_CHECK(map.size, 2);
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "bar"), "bar2");

    hashmap_deinit(&map);
}

MOS_TEST_CASE(hashmap_put_multiple)
{
    hashmap_t map = { 0 };
    const char *old;
    hashmap_common_type_init(&map, 135, string);
    MOS_TEST_CHECK(map.magic, HASHMAP_MAGIC);
    MOS_TEST_CHECK(map.capacity, 135);
    MOS_TEST_CHECK(map.size, 0);

    old = (char *) hashmap_put(&map, (ptr_t) "foo", (void *) "foo1");
    MOS_TEST_CHECK(old, NULL);
    MOS_TEST_CHECK(map.capacity, 135);
    MOS_TEST_CHECK(map.size, 1);
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "foo"), "foo1");

    old = (char *) hashmap_put(&map, (ptr_t) "foo", (void *) "foo2");
    MOS_TEST_CHECK(map.capacity, 135);
    MOS_TEST_CHECK(map.size, 1);
    MOS_TEST_CHECK_STRING(old, "foo1");
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "foo"), "foo2");

    old = (char *) hashmap_put(&map, (ptr_t) "bar", (void *) "bar1");
    MOS_TEST_CHECK(old, NULL);
    MOS_TEST_CHECK(map.capacity, 135);
    MOS_TEST_CHECK(map.size, 2);
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "bar"), "bar1");
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "foo"), "foo2");

    old = (char *) hashmap_put(&map, (ptr_t) "bar", (void *) "bar2");
    MOS_TEST_CHECK(map.capacity, 135);
    MOS_TEST_CHECK(map.size, 2);
    MOS_TEST_CHECK_STRING(old, "bar1");
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "bar"), "bar2");
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "foo"), "foo2");
    hashmap_deinit(&map);
}

MOS_TEST_CASE(hashmap_put_overflow)
{
    hashmap_t map = { 0 };
    const char *old;
    hashmap_common_type_init(&map, 1, string);
    MOS_TEST_CHECK(map.magic, HASHMAP_MAGIC);
    MOS_TEST_CHECK(map.capacity, 1);
    MOS_TEST_CHECK(map.size, 0);

    old = (char *) hashmap_put(&map, (ptr_t) "foo", (void *) "foo1");
    MOS_TEST_CHECK(old, NULL);
    MOS_TEST_CHECK(map.capacity, 1);
    MOS_TEST_CHECK(map.size, 1);
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "foo"), "foo1");

    old = (char *) hashmap_put(&map, (ptr_t) "bar", (void *) "bar1");
    MOS_TEST_CHECK(old, NULL);
    MOS_TEST_CHECK(map.capacity, 1);
    MOS_TEST_CHECK(map.size, 2);
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "bar"), "bar1");
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "foo"), "foo1");

    old = (char *) hashmap_put(&map, (ptr_t) "bar", (void *) "bar2");
    MOS_TEST_CHECK_STRING(old, "bar1");
    MOS_TEST_CHECK(map.capacity, 1);
    MOS_TEST_CHECK(map.size, 2);
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "bar"), "bar2");
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "foo"), "foo1");

    hashmap_deinit(&map);
}

MOS_TEST_CASE(hashmap_remove_function)
{
    const char *old;
    hashmap_t map = { 0 };
    hashmap_common_type_init(&map, 10, string);
    MOS_TEST_CHECK(map.magic, HASHMAP_MAGIC);
    MOS_TEST_CHECK(map.capacity, 10);
    MOS_TEST_CHECK(map.size, 0);

    old = (char *) hashmap_put(&map, (ptr_t) "foo", (void *) "foo1");
    MOS_TEST_CHECK(map.capacity, 10);
    MOS_TEST_CHECK(map.size, 1);
    MOS_TEST_CHECK(old, NULL);
    MOS_TEST_CHECK_STRING((const char *) hashmap_get(&map, (ptr_t) "foo"), "foo1");

    old = (char *) hashmap_remove(&map, (ptr_t) "foo");
    MOS_TEST_CHECK(map.capacity, 10);
    MOS_TEST_CHECK(map.size, 0);
    MOS_TEST_CHECK_STRING(old, "foo1");
    const char *nothing = (char *) hashmap_get(&map, (ptr_t) "foo");
    MOS_TEST_CHECK(nothing, NULL);

    old = (char *) hashmap_remove(&map, (ptr_t) "foo");
    MOS_TEST_CHECK(old, NULL);
    MOS_TEST_CHECK(map.capacity, 10);
    MOS_TEST_CHECK(map.size, 0);

    MOS_TEST_CHECK(hashmap_get(&map, (ptr_t) "foo"), NULL);
    hashmap_deinit(&map);
}

static size_t test_hashmap_foreach_count = 0;

bool test_foreach_function(uintn key, void *value, void *data)
{
    MOS_UNUSED(key);
    MOS_UNUSED(value);
    MOS_UNUSED(data);
    test_hashmap_foreach_count++;
    return true;
}

bool test_foreach_stop_at_quux(uintn key, void *value, void *data)
{
    MOS_UNUSED(data);
    MOS_UNUSED(value);
    test_hashmap_foreach_count++;
    if (strcmp((char *) key, "quux") == 0)
        return false;
    return true;
}

MOS_TEST_CASE(hashmap_foreach_function)
{
    hashmap_t map = { 0 };
    hashmap_common_type_init(&map, 10, string);
    MOS_TEST_CHECK(map.magic, HASHMAP_MAGIC);
    MOS_TEST_CHECK(map.capacity, 10);
    MOS_TEST_CHECK(map.size, 0);
    hashmap_put(&map, (ptr_t) "foo", (void *) "foo1");
    hashmap_put(&map, (ptr_t) "bar", (void *) "bar1");
    hashmap_put(&map, (ptr_t) "baz", (void *) "baz1");
    hashmap_put(&map, (ptr_t) "qux", (void *) "qux1");
    hashmap_put(&map, (ptr_t) "quux", (void *) "quux1");
    hashmap_put(&map, (ptr_t) "corge", (void *) "corge1");
    hashmap_put(&map, (ptr_t) "grault", (void *) "grault1");
    hashmap_put(&map, (ptr_t) "garply", (void *) "garply1");
    hashmap_put(&map, (ptr_t) "waldo", (void *) "waldo1");
    hashmap_put(&map, (ptr_t) "fred", (void *) "fred1");
    hashmap_put(&map, (ptr_t) "plugh", (void *) "plugh1");
    hashmap_put(&map, (ptr_t) "xyzzy", (void *) "xyzzy1");

    test_hashmap_foreach_count = 0;
    hashmap_foreach(&map, test_foreach_function, NULL);
    MOS_TEST_CHECK(test_hashmap_foreach_count, map.size);

    test_hashmap_foreach_count = 0;
    hashmap_foreach(&map, test_foreach_stop_at_quux, NULL);
    MOS_TEST_CHECK(test_hashmap_foreach_count, 4);
    hashmap_deinit(&map);
}
