// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/tree.h"

#include "lib/containers.h"
#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

void tree_add_child(tree_node_t *parent, tree_node_t *child)
{
    MOS_ASSERT(parent != NULL);
    MOS_ASSERT(child != NULL);
    MOS_ASSERT(child->parent == NULL);
    MOS_ASSERT(child->n_children == 0);
    MOS_ASSERT(child->children == NULL);
    child->parent = parent;
    parent->n_children++;
    parent->children = krealloc(parent->children, parent->n_children * sizeof(tree_node_t *));
    parent->children[parent->n_children - 1] = child;
}

void tree_remove_if(tree_node_t *node, bool (*predicate)(const tree_node_t *node))
{
    MOS_ASSERT(node != NULL);
    MOS_ASSERT(predicate != NULL);
    // TODO
}

const tree_node_t *tree_find_child_by_name(tree_op_t *op, const tree_node_t *node, const char *name, size_t name_len)
{
    if (node == NULL)
        return NULL;
    if (name_len == 0)
        return node;
    for (size_t i = 0; i < node->n_children; i++)
    {
        const tree_node_t *child = node->children[i];

        char *child_name = NULL;
        size_t child_name_len = 0;
        op->get_node_name(child, name_len, &child_name, &child_name_len);
        if (strncmp(child_name, name, child_name_len) == 0)
            return child;
    }
    return NULL;
}

const tree_node_t *tree_find_common_prefix(const tree_node_t *node1, const tree_node_t *node2)
{
    if (node1 == NULL || node2 == NULL)
        return NULL;
    if (node1 == node2)
        return node1;
    if (node1->parent == NULL || node2->parent == NULL)
        return NULL;
    if (node1->parent == node2->parent)
        return node1->parent;
    return tree_find_common_prefix(node1->parent, node2->parent);
}

void tree_trace_to_root(const tree_node_t *node, void (*trace_func)(const tree_node_t *node))
{
    if (node == NULL)
        return;
    trace_func(node);
    tree_trace_to_root(node->parent, trace_func);
}
