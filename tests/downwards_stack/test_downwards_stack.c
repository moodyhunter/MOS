// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/stack.h"
#include "mos/mm/kmalloc.h"
#include "test_engine.h"

MOS_TEST_CASE(stack_init_deinit)
{
    downwards_stack_t stack;
    stack_init(&stack, (void *) 12345, 6789);
    MOS_TEST_CHECK(stack.base, (void *) (12345 + 6789));
    MOS_TEST_CHECK(stack.head, (void *) (12345 + 6789));
    MOS_TEST_CHECK(stack.capacity, 6789);
    stack_deinit(&stack);

    MOS_TEST_CHECK(stack.base, NULL);
    MOS_TEST_CHECK(stack.head, NULL);
    MOS_TEST_CHECK(stack.capacity, 0);
}

MOS_TEST_CASE(stack_push_pop_stack)
{
    downwards_stack_t stack = { 0 };
    void *addr = kmalloc(4096);
    stack_init(&stack, addr, 4096);

    uintptr_t stack_bottom = (uintptr_t) addr + 4096;
    MOS_TEST_CHECK(stack.capacity, 4096);
    MOS_TEST_CHECK(stack.head, (void *) stack_bottom);
    MOS_TEST_CHECK(stack.base, (void *) stack_bottom);

    int pushed_1[10] = { 12345, 54321, 67890, 98765, 43210, 56789, 1234, 54321, 67890, 98765 };

    stack_push(&stack, pushed_1, sizeof(pushed_1));
    MOS_TEST_CHECK(stack.base, (void *) stack_bottom);
    MOS_TEST_CHECK(stack.head, (void *) (stack_bottom - sizeof(pushed_1)));
    MOS_TEST_CHECK(stack.capacity, 4096);

    int pushed_2[10] = { 4444, 5555, 6666, 7777, 8888, 9999, 10101, 11011, 12012, 13013 };
    stack_push(&stack, pushed_2, sizeof(pushed_2));
    MOS_TEST_CHECK(stack.base, (void *) stack_bottom);
    MOS_TEST_CHECK(stack.head, (void *) (stack_bottom - sizeof(pushed_1) - sizeof(pushed_2)));
    MOS_TEST_CHECK(stack.capacity, 4096);

    int tmp[10] = { 0 };
    stack_pop(&stack, tmp, sizeof(tmp));
    MOS_TEST_CHECK(stack.base, (void *) stack_bottom);
    MOS_TEST_CHECK(stack.head, (void *) (stack_bottom - sizeof(pushed_1)));
    MOS_TEST_CHECK(stack.capacity, 4096);
    MOS_TEST_CHECK(tmp[0], pushed_2[0]);
    MOS_TEST_CHECK(tmp[1], pushed_2[1]);
    MOS_TEST_CHECK(tmp[2], pushed_2[2]);
    MOS_TEST_CHECK(tmp[3], pushed_2[3]);
    MOS_TEST_CHECK(tmp[4], pushed_2[4]);
    MOS_TEST_CHECK(tmp[5], pushed_2[5]);
    MOS_TEST_CHECK(tmp[6], pushed_2[6]);
    MOS_TEST_CHECK(tmp[7], pushed_2[7]);
    MOS_TEST_CHECK(tmp[8], pushed_2[8]);
    MOS_TEST_CHECK(tmp[9], pushed_2[9]);

    stack_pop(&stack, tmp, sizeof(tmp));
    MOS_TEST_CHECK(stack.base, (void *) stack_bottom);
    MOS_TEST_CHECK(stack.head, (void *) stack_bottom);
    MOS_TEST_CHECK(stack.capacity, 4096);
    MOS_TEST_CHECK(tmp[0], pushed_1[0]);
    MOS_TEST_CHECK(tmp[1], pushed_1[1]);
    MOS_TEST_CHECK(tmp[2], pushed_1[2]);
    MOS_TEST_CHECK(tmp[3], pushed_1[3]);
    MOS_TEST_CHECK(tmp[4], pushed_1[4]);
    MOS_TEST_CHECK(tmp[5], pushed_1[5]);
    MOS_TEST_CHECK(tmp[6], pushed_1[6]);
    MOS_TEST_CHECK(tmp[7], pushed_1[7]);
    MOS_TEST_CHECK(tmp[8], pushed_1[8]);
    MOS_TEST_CHECK(tmp[9], pushed_1[9]);

    stack_deinit(&stack);
    kfree(addr);
}
