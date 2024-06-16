// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/lib/structures/list.h>
#include <mos/lib/structures/tree.h>
#include <mos/moslib_global.h>
#include <mos_string.h>

void tree_node_init(tree_node_t *node)
{
    linked_list_init(&node->list_node);
    linked_list_init(&node->children);
}

void tree_add_child(tree_node_t *parent, tree_node_t *child)
{
    MOS_LIB_ASSERT(parent != NULL);
    MOS_LIB_ASSERT(child != NULL);
    MOS_LIB_ASSERT(child->parent == NULL);

    child->parent = parent;
    MOS_LIB_ASSERT_X(list_is_empty(&child->children), "Child node already has children");

    linked_list_init(&child->children);
    list_node_append(&parent->children, &child->list_node);
}
