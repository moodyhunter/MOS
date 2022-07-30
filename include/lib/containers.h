// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"

#include <stddef.h>

#define container_of(ptr, type, member) ((type *) ((char *) (ptr) - (offsetof(type, member))))

typedef struct list_node
{
    struct list_node *next;
    struct list_node *prev;
} list_node_t;

#define as_linked_list list_node_t __list_node

#define list_get_node(element) element->__list_node

#define list_element(node, type) container_of(node, type, __list_node)
#define list_next(element)       list_element(list_get_node(element).next, __typeof(*element))
#define list_prev(element)       list_element(list_get_node(element).prev, __typeof(*element))

#define list_foreach(head, var) for (__typeof(*head) *var = head; var != NULL; var = list_next(var))

// TODO: implement list_foreach_safe
#define list_prepend(current, item)       _list_prepend(list_get_node(head), list_get_node(item))
#define list_append(current, item)        _list_append(list_get_node(current), list_get_node(item))
#define list_insert_after(current, item)  _list_insert_after(list_get_node(current), list_get_node(item))
#define list_insert_before(current, item) _list_insert_before(list_get_node(current), list_get_node(item))

void _list_append(list_node_t *head, list_node_t *node);
void _list_prepend(list_node_t *head, list_node_t *node);

void _list_insert_after(list_node_t *element, list_node_t *node);
void _list_insert_before(list_node_t *element, list_node_t *node);
