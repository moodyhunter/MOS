// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "test_engine.h"

MOS_TEST_CASE(kmalloc_single)
{
    void *p = kmalloc(1024);
    MOS_TEST_ASSERT(p != NULL, "kmalloc failed");
    memset(p, 0, 1024);
    kfree(p);
}

MOS_TEST_CASE(kmalloc_stress)
{
    void *p;
    int i;
    for (i = 0; i < 100; i++)
    {
        p = kmalloc(1024);
        MOS_TEST_ASSERT(p != NULL, "kmalloc failed");
        memset(p, 0, 1024);
        kfree(p);
    }
}

MOS_TEST_CASE(kmalloc_large)
{
    void *p = kmalloc(1024 * 1024);
    MOS_TEST_ASSERT(p != NULL, "kmalloc failed");
    memset(p, 0, 1024 * 1024);
    kfree(p);

    p = kmalloc(1024 * 1024 * 1024);
    MOS_TEST_ASSERT(p != NULL, "kmalloc failed");
    memset(p, 0, 1024 * 1024 * 1024);
    kfree(p);
}

MOS_TEST_CASE(kmalloc_a_lot)
{
    void *pointers[100];
    for (int t = 0; t < 20; t++)
    {
        for (int i = 0; i < 50; i++)
        {
            pointers[i] = kmalloc(71);
            MOS_TEST_ASSERT(pointers[i] != NULL, "failed to allocate memory");
            memset(pointers[i], 0, 71);
        }

        for (int i = 0; i < 50; i++)
        {
            kfree(pointers[i]);
        }
    }
}
