// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

#define container_of(ptr, type, member) ((type *) ((char *) (ptr) - (offsetof(type, member))))

typedef struct list_node_t list_node_t;

struct list_node_t
{
    list_node_t *prev;
    list_node_t *next;
};

#define as_linked_list list_node_t list_node

// clang-format off
#define LIST_HEAD_INIT(container) { .prev = &(container), .next = &(container) }
// clang-format on

#define LIST_NODE_INIT(container) LIST_HEAD_INIT(container.list_node)
#define list_entry(node, type)    container_of(((list_node_t *) node), type, list_node)
#define list_node(element)        (&((element)->list_node))

#define list_prepend(element, item)       list_node_prepend(list_node(element), list_node(item))
#define list_append(element, item)        list_node_append(list_node(element), list_node(item))
#define list_insert_before(element, item) list_node_insert_before(list_node(element), list_node(item))
#define list_insert_after(element, item)  list_node_insert_after(list_node(element), list_node(item))
#define list_remove(element)              list_node_remove(list_node(element))

#define list_foreach(t, v, h) for (t *v = list_entry((h).next, t); list_node(v) != &(h); v = list_entry(list_node(v)->next, t))

void linked_list_init(list_node_t *head_node);
bool list_is_empty(list_node_t *list);
void list_node_remove(list_node_t *link);

void list_node_prepend(list_node_t *head, list_node_t *item);
void list_node_append(list_node_t *head, list_node_t *item);
void list_node_insert_before(list_node_t *element, list_node_t *item);
void list_node_insert_after(list_node_t *element, list_node_t *item);
