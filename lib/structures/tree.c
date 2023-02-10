// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/tree.h"

#include "lib/liballoc.h"
#include "lib/mos_lib.h"
#include "lib/string.h"
#include "lib/structures/list.h"

void tree_add_child(tree_node_t *parent, tree_node_t *child)
{
    MOS_LIB_ASSERT(parent != NULL);
    MOS_LIB_ASSERT(child != NULL);
    MOS_LIB_ASSERT(child->parent == NULL);

    child->parent = parent;

    linked_list_init(&child->children);
    list_node_append(&parent->children, &child->list_node);
}

void tree_remove_if(tree_node_t *node, bool (*predicate)(const tree_node_t *node))
{
    MOS_LIB_ASSERT(node != NULL);
    MOS_LIB_ASSERT(predicate != NULL);
    // TODO
    MOS_LIB_UNIMPLEMENTED("tree_remove_if");
}

const tree_node_t *tree_find_child_by_name(tree_op_t *op, const tree_node_t *node, const char *name, size_t name_len)
{
    if (node == NULL)
        return NULL;
    if (name_len == 0)
        return node;

    list_node_foreach(child_node, &node->children)
    {
        const tree_node_t *child = container_of(child_node, tree_node_t, children);
        char *child_name = NULL;
        size_t child_name_len = 0;
        op->get_node_name(child, &child_name, &child_name_len);
        if (name_len == child_name_len && strncmp(name, child_name, name_len) == 0)
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
    if (node1->parent == node2)
        return node2;
    if (node2->parent == node1)
        return node1;
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
