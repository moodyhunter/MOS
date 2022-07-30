// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/containers.h"
#include "test_engine.h"

typedef struct
{
    int value_before;
    as_linked_list;
    int value_after;
} test_structure;

MOS_TEST_CASE(linked_list_append)
{
    MOS_TEST_CHECK(true, true);
}
