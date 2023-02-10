// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/list.h"
#include "mos/types.h"

typedef struct tree_node tree_node_t;

typedef struct tree_node
{
    as_linked_list;
    tree_node_t *parent;
    list_node_t children;
} tree_node_t;

/**
 * @brief Embed a tree node into a struct
 */
#define as_tree tree_node_t tree_node

typedef const struct
{
    void (*get_node_name)(const tree_node_t *node, char **name, size_t *name_len);
} tree_op_t;

#define tree_entry(node, type)  container_of((node), type, tree_node)
#define tree_node(element)      (&((element)->tree_node))
#define tree_parent(node, type) (tree_entry(tree_node(node)->parent, type))

#define tree_children_list(node)     (&((node)->tree_node.children))
#define tree_child_entry(node, type) container_of(node, type, tree_node.list_node)
#define tree_child_node(node)        (&((node)->tree_node.list_node))

#define tree_foreach_child(t, v, h)                                                                                                                                      \
    for (t *v = tree_child_entry(tree_children_list(h)->next, t); tree_child_node(v) != tree_children_list(h); v = tree_child_entry(tree_child_node(v)->next, t))

void tree_add_child(tree_node_t *parent, tree_node_t *child);
void tree_remove_if(tree_node_t *node, bool (*predicate)(const tree_node_t *node));

const tree_node_t *tree_find_child_by_name(tree_op_t *op, const tree_node_t *node, const char *name, size_t name_len);
const tree_node_t *tree_find_common_prefix(const tree_node_t *node1, const tree_node_t *node2);

void tree_trace_to_root(const tree_node_t *node, void (*trace_func)(const tree_node_t *node));
