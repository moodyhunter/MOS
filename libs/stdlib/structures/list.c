// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/lib/structures/list.h>

// Note: The nullability of each parameter is not checked, because results will be the same no matter what.
// (i.e. kernel panic / process termination)

/**
 * @brief Initialise a circular double linked list
 * @post  head_node->next == head_node
 * @post  head_node->prev == head_node
 *
 * @param node The list head node (and thus, the only element in the newly created list)
 */
void linked_list_init(list_node_t *node)
{
    node->prev = node;
    node->next = node;
}

bool list_is_empty(const list_node_t *head)
{
    return head->next == head;
}

void list_node_remove(list_node_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;

    // detach the node from the list
    node->next = node;
    node->prev = node;
}

// ! Internal API
/**
 * @brief Insert a node into a list
 * @post  node->next == next
 * @post  node->prev == prev
 * @post  prev->next == node
 * @post  next->prev == node
 *
 * @param prev The node before the insertion point
 * @param new_node The node to insert
 * @param next The node after the insertion point
 */
static void list_node_insert(list_node_t *prev, list_node_t *new_node, list_node_t *next)
{
    new_node->prev = prev;
    new_node->next = next;
    prev->next = new_node;
    next->prev = new_node;
}

list_node_t *list_node_pop(list_node_t *head)
{
    list_node_t *node = head->next;
    list_node_remove(node);
    return node;
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
