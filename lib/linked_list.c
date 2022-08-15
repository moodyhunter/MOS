// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/containers.h"

void linked_list_init(list_node_t *node)
{
    node->prev = node;
    node->next = node;
}

bool list_is_empty(list_node_t *head)
{
    return head->next == head;
}

void list_node_remove(list_node_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;

    // detach the node from the list
    // ! Is this really necessary?
    node->next = node;
    node->prev = node;
}

// ! Internal API
static void list_node_insert(list_node_t *prev, list_node_t *new_node, list_node_t *next)
{
    new_node->prev = prev;
    new_node->next = next;
    prev->next = new_node;
    next->prev = new_node;
}

void list_node_prepend(list_node_t *head, list_node_t *item)
{
    list_node_insert(head, item, head->next);
}

void list_node_append(list_node_t *head, list_node_t *item)
{
    // The list is circular, so accessing the list tail is like the prev of its head.
    list_node_insert(head->prev, item, head);
}

void list_node_insert_before(list_node_t *element, list_node_t *item)
{
    list_node_insert(element->prev, item, element);
}

void list_node_insert_after(list_node_t *element, list_node_t *item)
{
    list_node_insert(element, item, element->next);
}
