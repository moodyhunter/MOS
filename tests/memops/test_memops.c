// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_engine.h"

#include <stdlib.h>
#include <string.h>

MOS_TEST_CASE(test_lib_memcpy)
{
    char *src = kcalloc(500, sizeof(char));
    char *dst = kcalloc(500, sizeof(char));

    for (size_t i = 0; i < 500; i++)
        src[i] = i;

    memcpy(dst, src, 500);

    for (size_t i = 0; i < 500; i++)
        MOS_TEST_CHECK(dst[i] == src[i], true);

    kfree(src);
    kfree(dst);
}

MOS_TEST_CASE(test_lib_memmove_simple)
{
    char *src = kcalloc(500, sizeof(char));
    char *dst = kcalloc(500, sizeof(char));

    for (size_t i = 0; i < 500; i++)
        src[i] = i;

    memmove(dst, src, 500);
    for (size_t i = 0; i < 500; i++)
        MOS_TEST_CHECK(dst[i] == src[i], true);

    kfree(src);
    kfree(dst);
}

MOS_TEST_CASE(test_memmove_overlapped)
{
    size_t *src = kcalloc(500, sizeof(size_t));

    for (size_t i = 0; i < 300; i++)
        src[i] = i;

    memmove(src + 200, src, 300 * sizeof(size_t));

    for (size_t i = 200; i < 500; i++)
        MOS_TEST_CHECK(src[i], i - 200);

    kfree(src);
}

MOS_TEST_CASE(test_memmove_overlapped_backwards)
{
    size_t *src = kcalloc(500, sizeof(size_t));

    for (size_t i = 0; i < 500; i++)
        src[i] = i;

    // |  0 -  99 - 100 - 199 - 200 - 299| - 300 - 399 - 400 - 499
    memmove(src, src + 200, 300 * sizeof(size_t));
    // |200 - 299 - 300 - 399 - 400 - 499| - 300 - 399 - 400 - 499

    // the front 200 ~ 499 area
    for (size_t i = 0; i < 300; i++)
        MOS_TEST_CHECK(src[i], i + 200);

    // 300 ~ 499 area
    for (size_t i = 300; i < 500; i++)
        MOS_TEST_CHECK(src[i], i);
    kfree(src);
}
