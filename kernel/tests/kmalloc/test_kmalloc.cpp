// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_engine_impl.h"

#include <mos_stdlib.hpp>
#include <mos_string.hpp>

MOS_TEST_CASE(kmalloc_single)
{
    void *p = kcalloc<char>(1024);
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
        p = kcalloc<char>(1024);
        MOS_TEST_ASSERT(p != NULL, "kmalloc failed");
        memset(p, 0, 1024);
        kfree(p);
    }
}

MOS_TEST_CASE(kmalloc_large)
{
    char *p = 0;
    p = kcalloc<char>(1 MB);
    MOS_TEST_ASSERT(p != NULL, "kmalloc failed");
    memset(p, 0, 1 MB);
    kfree(p);

    p = kcalloc<char>(100 MB);
    MOS_TEST_ASSERT(p != NULL, "kmalloc failed");
    memset(p, 0, 100 MB);
    kfree(p);

    // we won't test larger allocations because for (32-bit) x86, the kernel
    // heap starts at 0xd0000000, while the initrd is placed at 0xec000000,
    // That only leaves 0x1c000000 bytes for the kernel heap i.e. ~460 MB.
}

MOS_TEST_CASE(kmalloc_a_lot)
{
    char *pointers[100];
    for (int t = 0; t < 20; t++)
    {
        for (int i = 0; i < 50; i++)
        {
            pointers[i] = kcalloc<char>(71);
            MOS_TEST_ASSERT(pointers[i] != NULL, "failed to allocate memory");
            memset(pointers[i], 0, 71);
        }

        for (int i = 0; i < 50; i++)
        {
            kfree(pointers[i]);
        }
    }
}
