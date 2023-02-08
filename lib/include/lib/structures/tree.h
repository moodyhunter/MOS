// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/list.h"
#include "mos/types.h"

typedef struct tree_node tree_node_t;

typedef struct tree_node
{
    tree_node_t *parent;
    tree_node_t *first_child;
    list_node_t siblings;
} tree_node_t;

/**
 * @brief Embed a tree node into a struct
 */
#define as_tree tree_node_t tree_node

typedef const struct
{
    void (*get_node_name)(const tree_node_t *node, char **name, size_t *name_len);
} tree_op_t;

#define tree_entry(node, type) container_of((node), type, tree_node)
#define tree_node(element)     (&((element)->tree_node))

#define _tree_sibling_entry(node, type) container_of((node), type, tree_node.siblings)
#define _tree_sibling_list_node(node)   (&(node)->tree_node.siblings)

#define tree_foreach_child(type, var, node)                                                                                                                              \
    for (type *var = _tree_sibling_entry((node)->tree_node.first_child, type); _tree_sibling_list_node(var) != (&(node)->tree_node.siblings);                            \
         var = _tree_sibling_entry(_tree_sibling_list_node(var)->next, type))

void tree_add_child(tree_node_t *parent, tree_node_t *child);
void tree_remove_if(tree_node_t *node, bool (*predicate)(const tree_node_t *node));

const tree_node_t *tree_find_child_by_name(tree_op_t *op, const tree_node_t *node, const char *name, size_t name_len);
const tree_node_t *tree_find_common_prefix(const tree_node_t *node1, const tree_node_t *node2);

void tree_trace_to_root(const tree_node_t *node, void (*trace_func)(const tree_node_t *node));
