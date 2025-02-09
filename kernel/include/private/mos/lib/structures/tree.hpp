// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/lib/structures/list.hpp>
#include <mos/moslib_global.hpp>
#include <mos/types.hpp>

typedef struct tree_node tree_node_t;

typedef struct tree_node
{
    as_linked_list;
    tree_node_t *parent;
    list_head children;
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
#define tree_child_entry(node, type) container_of(container_of((node), tree_node_t, list_node), type, tree_node)
#define tree_child_node(node)        (&((node)->tree_node.list_node))

#define tree_foreach_child(t, v, h)                                                                                                                                      \
    for (auto v = tree_child_entry(tree_children_list(h)->next, t); tree_child_node(v) != tree_children_list(h); v = tree_child_entry(tree_child_node(v)->next, t))

MOSAPI void tree_node_init(tree_node_t *node);
MOSAPI void tree_add_child(tree_node_t *parent, tree_node_t *child);
