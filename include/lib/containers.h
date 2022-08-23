// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

#define container_of(ptr, type, member) ((type *) ((char *) (ptr) - (offsetof(type, member))))

typedef struct list_node list_node_t;
typedef struct tree_node tree_node_t;

struct list_node
{
    list_node_t *prev;
    list_node_t *next;
};

typedef struct tree_node
{
    tree_node_t *parent;
    size_t n_children;
    tree_node_t **children;
} tree_node_t;

#define as_linked_list list_node_t list_node
#define as_tree        tree_node_t tree_node

// clang-format off
#define LIST_HEAD_INIT(container) { .prev = &(container), .next = &(container) }
// clang-format on

#define LIST_NODE_INIT(container) LIST_HEAD_INIT(container.list_node)
