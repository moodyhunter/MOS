// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/mos_lib.h"
#include "mos/types.h"

/**
 * @defgroup linked_list libs.LinkedList
 * @ingroup libs
 * @brief A circular, doubly-linked list.
 * @{
 */

typedef struct list_node list_node_t;

/** @brief A node in a linked list. */
struct list_node
{
    list_node_t *prev;
    list_node_t *next;
};

/**
 * @brief Embed a list node into a struct
 */
#define as_linked_list list_node_t list_node

// clang-format off
#define LIST_HEAD_INIT(container) { .prev = &(container), .next = &(container) }
// clang-format on

#define LIST_NODE_INIT(container) LIST_HEAD_INIT(container.list_node)

/**
 * @brief Get the container struct of a list node
 */
#define list_entry(node, type) const_container_of((node), list_node_t, type, list_node)

/**
 * @brief Get the `list_node' of a list element.
 * This is exactly the reverse of `list_entry' above.
 */
#define list_node(element) (&((element)->list_node))

#define list_prepend(element, item)       list_node_prepend(list_node(element), list_node(item))
#define list_append(element, item)        list_node_append(list_node(element), list_node(item))
#define list_insert_before(element, item) list_node_insert_before(list_node(element), list_node(item))
#define list_insert_after(element, item)  list_node_insert_after(list_node(element), list_node(item))
#define list_remove(element)              list_node_remove(list_node(element))

/**
 * @brief Iterate over a list
 *
 * @param t Type of the list elements (e.g. `struct foo')
 * @param v Name of the variable to use for the current element (e.g. `item')
 * @param h List Head (e.g. `consoles')
 */
#define list_foreach(t, v, h)   for (__typeof(list_entry(&(h), t)) v = list_entry((h).next, t); list_node(v) != &(h); v = list_entry(list_node(v)->next, t))
#define list_node_foreach(v, h) for (__typeof(h) v = (h)->next; v != (h); v = v->next)

#define list_foreach_reverse(t, v, h)         for (t *v = list_entry((h).prev, t); list_node(v) != &(h); v = list_entry(list_node(v)->prev, t))
#define list_node_foreach_reverse(node, head) for (list_node_t *node = (head)->prev; node != (head); node = node->prev)

MOSAPI void linked_list_init(list_node_t *head_node);
MOSAPI bool list_is_empty(list_node_t *head);
MOSAPI void list_node_remove(list_node_t *link);

MOSAPI list_node_t *list_node_pop(list_node_t *head);
MOSAPI void list_node_prepend(list_node_t *head, list_node_t *item);
MOSAPI void list_node_append(list_node_t *head, list_node_t *item);
MOSAPI void list_node_insert_before(list_node_t *element, list_node_t *item);
MOSAPI void list_node_insert_after(list_node_t *element, list_node_t *item);

/** @} */
