// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_engine_impl.h"

#include <mos/lib/structures/list.hpp>

struct test_structure
{
    int value_before;
    as_linked_list;
    int value_after;

    test_structure(int value_before, int value_after) : value_before(value_before), value_after(value_after) {};
};

MOS_TEST_CASE(test_list_init)
{
    test_structure pp = { 0, 1 };

    MOS_TEST_CHECK(pp.value_before, 0);
    MOS_TEST_CHECK(pp.value_after, 1);

    MOS_TEST_CHECK(pp.list_node.prev, &pp.list_node);
    MOS_TEST_CHECK(pp.list_node.next, &pp.list_node);

    list_node_t head;
    MOS_TEST_CHECK(head.next, &head);
    MOS_TEST_CHECK(head.prev, &head);

    // Check is_empty() on an empty list
    bool is_empty = list_is_empty(&head);
    MOS_TEST_CHECK(is_empty, true);

    // Check is_empty() on a non-empty list
    test_structure p1 = { 0, 1 };
    list_node_append(&head, &p1.list_node);
    is_empty = list_is_empty(&head);
    MOS_TEST_CHECK(is_empty, false);
}

MOS_TEST_CASE(test_list_node_append)
{
    list_node_t list_head;
    test_structure s1 = { 1, 2 };
    test_structure s2 = { 3, 4 };
    test_structure s3 = { 5, 6 };
    test_structure s4 = { 7, 8 };
    test_structure s5 = { 9, 10 };
    list_node_append(&list_head, &s1.list_node); // list_head -> s1
    list_node_append(&list_head, &s2.list_node); // list_head -> s1 -> s2
    list_node_append(&list_head, &s3.list_node); // list_head -> s1 -> s2 -> s3
    list_node_append(&list_head, &s4.list_node); // list_head -> s1 -> s2 -> s3 -> s4
    list_node_append(&list_head, &s5.list_node); // list_head -> s1 -> s2 -> s3 -> s4 -> s5

    // next pointer
    MOS_TEST_CHECK(list_head.next, &s1.list_node);
    MOS_TEST_CHECK(s1.list_node.next, &s2.list_node);
    MOS_TEST_CHECK(s2.list_node.next, &s3.list_node);
    MOS_TEST_CHECK(s3.list_node.next, &s4.list_node);
    MOS_TEST_CHECK(s4.list_node.next, &s5.list_node);
    MOS_TEST_CHECK(s5.list_node.next, &list_head);

    // prev pointer
    MOS_TEST_CHECK(list_head.prev, &s5.list_node);
    MOS_TEST_CHECK(s5.list_node.prev, &s4.list_node);
    MOS_TEST_CHECK(s4.list_node.prev, &s3.list_node);
    MOS_TEST_CHECK(s3.list_node.prev, &s2.list_node);
    MOS_TEST_CHECK(s2.list_node.prev, &s1.list_node);
    MOS_TEST_CHECK(s1.list_node.prev, &list_head);
}

// prepending is really the same as appending to the head.
MOS_TEST_CASE(test_list_node_prepend)
{
    list_node_t list_head;
    test_structure s1 = { 1, 2 };
    test_structure s2 = { 3, 4 };
    test_structure s3 = { 5, 6 };
    test_structure s4 = { 7, 8 };
    test_structure s5 = { 9, 10 };
    list_node_prepend(&list_head, &s5.list_node); // s5 -> list_head
    list_node_prepend(&list_head, &s4.list_node); // s4 -> s5 -> list_head
    list_node_prepend(&list_head, &s3.list_node); // s3 -> s4 -> s5 -> list_head
    list_node_prepend(&list_head, &s2.list_node); // s2 -> s3 -> s4 -> s5 -> list_head
    list_node_prepend(&list_head, &s1.list_node); // s1 -> s2 -> s3 -> s4 -> s5 -> list_head

    // next pointer
    MOS_TEST_CHECK(list_head.next, &s1.list_node);
    MOS_TEST_CHECK(s1.list_node.next, &s2.list_node);
    MOS_TEST_CHECK(s2.list_node.next, &s3.list_node);
    MOS_TEST_CHECK(s3.list_node.next, &s4.list_node);
    MOS_TEST_CHECK(s4.list_node.next, &s5.list_node);
    MOS_TEST_CHECK(s5.list_node.next, &list_head);

    // prev pointer
    MOS_TEST_CHECK(list_head.prev, &s5.list_node);
    MOS_TEST_CHECK(s5.list_node.prev, &s4.list_node);
    MOS_TEST_CHECK(s4.list_node.prev, &s3.list_node);
    MOS_TEST_CHECK(s3.list_node.prev, &s2.list_node);
    MOS_TEST_CHECK(s2.list_node.prev, &s1.list_node);
    MOS_TEST_CHECK(s1.list_node.prev, &list_head);
}

MOS_TEST_CASE(test_list_node_insert)
{
    list_node_t list_head;
    test_structure s1 = { 1, 2 };
    test_structure s2 = { 3, 4 };
    test_structure s3 = { 5, 6 };
    test_structure s4 = { 7, 8 };
    test_structure s5 = { 9, 10 };

    test_structure new_s = { 11, 12 };
    test_structure new_s2 = { 13, 14 };

    // Connect the list.
    list_node_append(&list_head, &s1.list_node); // list_head -> s1
    list_node_append(&list_head, &s2.list_node); // list_head -> s1 -> s2
    list_node_append(&list_head, &s3.list_node); // list_head -> s1 -> s2 -> s3
    list_node_append(&list_head, &s4.list_node); // list_head -> s1 -> s2 -> s3 -> s4
    list_node_append(&list_head, &s5.list_node); // list_head -> s1 -> s2 -> s3 -> s4 -> s5

    // Insert a new node before s3.
    list_node_insert_before(&s3.list_node, &new_s.list_node); // s1 -> s2 -> new_s -> s3 -> s4 -> s5 -> list_head
    MOS_TEST_CHECK(new_s.list_node.next, &s3.list_node);
    MOS_TEST_CHECK(new_s.list_node.prev, &s2.list_node);
    MOS_TEST_CHECK(s2.list_node.next, &new_s.list_node);
    MOS_TEST_CHECK(s3.list_node.prev, &new_s.list_node);

    // original parts of the list should be unchanged.
    MOS_TEST_CHECK(s2.list_node.prev, &s1.list_node);
    MOS_TEST_CHECK(s3.list_node.next, &s4.list_node);

    // Insert a new node after s4
    list_node_insert_after(&s4.list_node, &new_s2.list_node); // s1 -> s2 -> new_s -> s3 -> s4 -> new_s2 -> s5 -> list_head
    MOS_TEST_CHECK(new_s2.list_node.next, &s5.list_node);
    MOS_TEST_CHECK(new_s2.list_node.prev, &s4.list_node);
    MOS_TEST_CHECK(s4.list_node.next, &new_s2.list_node);
    MOS_TEST_CHECK(s5.list_node.prev, &new_s2.list_node);

    // original parts of the list should be unchanged.
    MOS_TEST_CHECK(s4.list_node.prev, &s3.list_node);
    MOS_TEST_CHECK(s5.list_node.next, &list_head);
}

MOS_TEST_CASE(test_list_remove)
{
    list_node_t list_head;
    test_structure s1 = { 1, 2 };
    test_structure s2 = { 3, 4 };
    test_structure s3 = { 5, 6 };
    test_structure s4 = { 7, 8 };
    test_structure s5 = { 9, 10 };
    list_node_append(&list_head, &s1.list_node); // list_head -> s1
    list_node_append(&list_head, &s2.list_node); // list_head -> s1 -> s2
    list_node_append(&list_head, &s3.list_node); // list_head -> s1 -> s2 -> s3
    list_node_append(&list_head, &s4.list_node); // list_head -> s1 -> s2 -> s3 -> s4
    list_node_append(&list_head, &s5.list_node); // list_head -> s1 -> s2 -> s3 -> s4 -> s5

    // Remove s3.
    list_node_remove(&s3.list_node); // list_head -> s1 -> s2 -> s4 -> s5
    MOS_TEST_CHECK(s1.list_node.next, &s2.list_node);
    MOS_TEST_CHECK(s2.list_node.next, &s4.list_node);
    MOS_TEST_CHECK(s4.list_node.next, &s5.list_node);
    MOS_TEST_CHECK(s5.list_node.next, &list_head);
    MOS_TEST_CHECK(list_head.prev, &s5.list_node);

    MOS_TEST_CHECK(s1.list_node.prev, &list_head);
    MOS_TEST_CHECK(s2.list_node.prev, &s1.list_node);
    MOS_TEST_CHECK(s4.list_node.prev, &s2.list_node);
    MOS_TEST_CHECK(s5.list_node.prev, &s4.list_node);
    MOS_TEST_CHECK(list_head.next, &s1.list_node);
}

MOS_TEST_CASE(test_list_macros)
{
    list_node_t list_head;
    test_structure s1 = { 1, 2 };
    test_structure s2 = { 3, 4 };

    MOS_TEST_CHECK(list_entry(&s1.list_node, test_structure), &s1);
    MOS_TEST_CHECK(list_entry(&s2.list_node, test_structure)->value_before, s2.value_before);
    MOS_TEST_CHECK(list_entry(&s2.list_node, test_structure)->value_after, s2.value_after);

    MOS_TEST_CHECK(list_node(&s1), &s1.list_node);
}

MOS_TEST_CASE(test_list_foreach)
{
    list_node_t list_head;
    test_structure s1 = { 1, 2 };
    test_structure s2 = { 3, 4 };
    test_structure s3 = { 5, 6 };
    test_structure s4 = { 7, 8 };
    test_structure s5 = { 9, 10 };
    list_node_append(&list_head, &s1.list_node); // list_head -> s1
    list_node_append(&list_head, &s2.list_node); // list_head -> s1 -> s2
    list_node_append(&list_head, &s3.list_node); // list_head -> s1 -> s2 -> s3
    list_node_append(&list_head, &s4.list_node); // list_head -> s1 -> s2 -> s3 -> s4
    list_node_append(&list_head, &s5.list_node); // list_head -> s1 -> s2 -> s3 -> s4 -> s5
    int i = 0;
    list_node_t *node = list_head.next;
    while (node != &list_head)
    {
        i++;
        node = node->next;
    }
    MOS_TEST_CHECK(i, 5);

    // sum the list.
    int sum_before = 0;
    int sum_after = 0;
    list_foreach(test_structure, node, list_head)
    {
        sum_before += node->value_before;
        sum_after += node->value_after;
    }
    MOS_TEST_CHECK(sum_before, 25);
    MOS_TEST_CHECK(sum_after, 30);
}

MOS_TEST_CASE(test_list_headless_foreach)
{
    test_structure s1 = { 1, 2 };
    test_structure s2 = { 3, 4 };
    test_structure s3 = { 5, 6 };
    test_structure s4 = { 7, 8 };
    test_structure s5 = { 9, 10 };
    list_append(&s1, &s2); // s1 -> s2
    list_append(&s1, &s3); // s1 -> s2 -> s3
    list_append(&s1, &s4); // s1 -> s2 -> s3 -> s4
    list_append(&s1, &s5); // s1 -> s2 -> s3 -> s4 -> s5
    int i = 0;
    test_structure *_this = &s1;
    do
    {
        i++;
        _this = list_next_entry(_this, test_structure);
    } while (_this != &s1);
    MOS_TEST_CHECK(i, 5);

    // sum the list.
    int sum_before = 0;
    int sum_after = 0;
    list_headless_foreach(test_structure, node, s1)
    {
        sum_before += node->value_before;
        sum_after += node->value_after;
    }
    MOS_TEST_CHECK(sum_before, 25);
    MOS_TEST_CHECK(sum_after, 30);

    // sum the list, starting from s3.
    sum_before = 0;
    sum_after = 0;
    list_headless_foreach(test_structure, node, s3)
    {
        sum_before += node->value_before;
        sum_after += node->value_after;
    }
    MOS_TEST_CHECK(sum_before, 25);
    MOS_TEST_CHECK(sum_after, 30);

    // sum the list, starting from s5.
    sum_before = 0;
    sum_after = 0;
    list_headless_foreach(test_structure, node, s5)
    {
        sum_before += node->value_before;
        sum_after += node->value_after;
    }
    MOS_TEST_CHECK(sum_before, 25);
    MOS_TEST_CHECK(sum_after, 30);

    // reverse sum the list.
    sum_before = 0;
    sum_after = 0;
    list_headless_foreach_reverse(test_structure, node, s1)
    {
        sum_before += node->value_before;
        sum_after += node->value_after;
    }
    MOS_TEST_CHECK(sum_before, 25);
    MOS_TEST_CHECK(sum_after, 30);

    // reverse sum the list, starting from s3.
    sum_before = 0;
    sum_after = 0;
    list_headless_foreach_reverse(test_structure, node, s3)
    {
        sum_before += node->value_before;
        sum_after += node->value_after;
    }
    MOS_TEST_CHECK(sum_before, 25);
    MOS_TEST_CHECK(sum_after, 30);
}

MOS_TEST_CASE(test_list_safe_foreach)
{
    list_node_t list_head;
    test_structure s1 = { 1, 2 };
    test_structure s2 = { 3, 4 };
    test_structure s3 = { 5, 6 };
    test_structure s4 = { 7, 8 };
    test_structure s5 = { 9, 10 };
    list_node_append(&list_head, &s1.list_node); // list_head -> s1
    list_node_append(&list_head, &s2.list_node); // list_head -> s1 -> s2
    list_node_append(&list_head, &s3.list_node); // list_head -> s1 -> s2 -> s3
    list_node_append(&list_head, &s4.list_node); // list_head -> s1 -> s2 -> s3 -> s4
    list_node_append(&list_head, &s5.list_node); // list_head -> s1 -> s2 -> s3 -> s4 -> s5

    // count the list.
    size_t count = 0;
    list_node_t *node = list_head.next;
    while (node != &list_head)
    {
        count++;
        node = node->next;
    }
    MOS_TEST_CHECK(count, 5);

    // sum the list.
    int sum_before = 0;
    int sum_after = 0;
    list_foreach(test_structure, node, list_head)
    {
        sum_before += node->value_before;
        sum_after += node->value_after;
    }
    MOS_TEST_CHECK(sum_before, 25);
    MOS_TEST_CHECK(sum_after, 30);

    // sum the list, and in the loop, remove s3.
    sum_before = 0;
    sum_after = 0;
    list_foreach(test_structure, node, list_head)
    {
        sum_before += node->value_before;
        sum_after += node->value_after;
        if (node == &s3)
        {
            list_node_remove(&s3.list_node);
        }
    }
    MOS_TEST_CHECK(sum_before, 25);
    MOS_TEST_CHECK(sum_after, 30);

    // count the list.
    count = 0;
    list_foreach(test_structure, node, list_head)
    {
        count++;
    }
    MOS_TEST_CHECK(count, 4);
}
