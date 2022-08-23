// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/path.h"

#include "lib/containers.h"
#include "lib/string.h"
#include "lib/structures/tree.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"
#include "mos/types.h"

#define PATH_MAX_LENGTH 256

path_t root_path = {
    .name = "/",
    .tree_node = {
        .parent = NULL,
        .n_children = 0,
        .children = NULL,
    },
};

void path_node_get_name(const tree_node_t *node, size_t limit, char **name, size_t *name_len)
{
    path_t *path = tree_entry(node, path_t);
    *name = (char *) path->name;
    *name_len = strlen(path->name);
    MOS_ASSERT(*name_len <= limit);
}

tree_op_t path_tree_op = {
    .get_node_name = path_node_get_name,
};

path_t *construct_path(const char *path)
{
    path_t *current = &root_path;
    while (*path)
    {
        if (*path == '/')
        {
            path++;
            continue;
        }

        const char *name = path;
        while (*path && *path != '/')
            path++;

        const size_t name_len = path - name;
        name = alloc_string(name, name_len);

        const tree_node_t *cnode = tree_find_child_by_name(&path_tree_op, tree_node(current), name, name_len);

        path_t *child;
        if (cnode == NULL)
        {
            child = kmalloc(sizeof(path_t));
            child->name = name;
            tree_add_child(tree_node(current), tree_node(child));
        }
        else
        {
            child = tree_entry(cnode, path_t);
            kfree(name);
        }
        current = child;
    }
    return current;
}

bool path_verify_prefix(const path_t *path, const path_t *prefix)
{
    return tree_find_common_prefix(tree_node(path), tree_node(prefix)) == tree_node(prefix);
}

const char *path_get_full_path_string(const path_t *root, const path_t *leaf)
{
    if (root == NULL || leaf == NULL)
        return alloc_string("", 0);

    if (root == leaf)
        return alloc_string(root->name, strlen(root->name));

    const path_t *current = leaf;
    char *pool = kmalloc(PATH_MAX_LENGTH);
    char *begin = pool;

    do
    {
        size_t len = strlen(current->name);
        begin -= len;
        memcpy(begin, current->name, len);
        *(--begin) = '/';
        current = tree_entry(tree_node(current)->parent, path_t);
    } while (current && current != root);

    begin++; // remove leading '/'

    const char *result = alloc_string(begin, pool - begin);
    kfree(pool);
    return result;
}
