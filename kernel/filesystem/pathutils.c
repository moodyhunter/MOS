// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/pathutils.h"

#include "lib/string.h"
#include "lib/structures/tree.h"
#include "mos/filesystem/filesystem.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

void path_node_get_name(const tree_node_t *node, char **name, size_t *name_len)
{
    fsnode_t *path = tree_entry(node, fsnode_t);
    *name = (char *) path->name;
    *name_len = strlen(path->name);
}

static tree_op_t path_tree_op = {
    .get_node_name = path_node_get_name,
};

fsnode_t root_path = {
    .name = "/",
    .tree_node = {
        .parent = NULL,
        .n_children = 0,
        .children = NULL,
    },
};
// Private functions

fsnode_t *impl_path_get_subpath(fsnode_t *cwd, const char *path, size_t path_len)
{
    MOS_ASSERT(cwd != NULL);
    MOS_ASSERT(path != NULL);
    MOS_ASSERT(path[0] != PATH_SEPARATOR);

    if (path_len == 2 && strncmp(path, "..", 2) == 0)
    {
        fsnode_t *parent = path_parent(cwd);
        if (parent == NULL)
        {
            mos_warn("cannot go up from root");
            return &root_path;
        }
        return parent;
    }
    else if (path_len == 1 && strncmp(path, ".", 1) == 0)
    {
        // do nothing
        return cwd;
    }
    else
    {
        const tree_node_t *cnode = tree_find_child_by_name(&path_tree_op, tree_node(cwd), path, path_len);
        fsnode_t *child;
        if (cnode == NULL)
        {
            child = kzalloc(sizeof(fsnode_t));
            child->name = duplicate_string(path, path_len);
            tree_add_child(tree_node(cwd), tree_node(child));
        }
        else
        {
            child = tree_entry(cnode, fsnode_t);
        }
        return child;
    }
}

const char *path_next_segment(const char *path, size_t *segment_len)
{
    if (path == NULL || *path == '\0')
        return NULL;
    if (*path == PATH_SEPARATOR)
        path++;
    const char *next = strchr(path, PATH_SEPARATOR);
    if (next == NULL)
        next = path + strlen(path);
    *segment_len = next - path;
    return next;
}

fsnode_t *path_find_fsnode(const char *path)
{
    fsnode_t *target;
    bool resolved = path_resolve(&root_path, path, &target);
    if (!resolved)
    {
        mos_warn("path_contruct: path not resolved");
        return NULL;
    }
    return target;
}

bool path_resolve(fsnode_t *cwd, const char *path, fsnode_t **resolved)
{
    MOS_ASSERT(cwd && path && resolved);
    if (path == NULL || *path == '\0')
    {
        *resolved = cwd;
        return true;
    }

    fsnode_t *real_cwd = cwd;

    // an absolute path starts with a slash
    if (path[0] == PATH_SEPARATOR)
    {
        path++;
        real_cwd = &root_path;
    }

    size_t segment_len = 0;
    const char *next_seg = path_next_segment(path, &segment_len);

    fsnode_t *current = impl_path_get_subpath(real_cwd, path, segment_len);
    if (next_seg == NULL || *next_seg == '\0')
    {
        *resolved = current;
        return true;
    }

    file_stat_t stat = { 0 };
    vfs_path_stat(current, &stat);
    if (stat.type == FILE_TYPE_SYMLINK)
    {
        fsnode_t *symlink_target; // to be allocated by vfs_path_readlink (aka, **resolved)
        bool ok = vfs_path_readlink(current, &symlink_target);
        if (!ok)
        {
            *resolved = NULL;
            return false;
        }
        current = symlink_target;
    }

    return path_resolve(current, next_seg + 1, resolved);
}

bool path_verify_prefix(const fsnode_t *path, const fsnode_t *prefix)
{
    return tree_find_common_prefix(tree_node(path), tree_node(prefix)) == tree_node(prefix);
}

const char *path_to_string_relative(const fsnode_t *root, const fsnode_t *leaf)
{
    if (root == NULL || leaf == NULL)
        return strdup("");

    if (root == leaf)
        return strdup(root->name);

    const fsnode_t *current = leaf;
    char *pool = kmalloc(MOS_PATH_MAX_LENGTH);
    char *begin = pool + MOS_PATH_MAX_LENGTH;

    do
    {
        size_t len = strlen(current->name);
        begin -= len;
        memcpy(begin, current->name, len);
        *(--begin) = '/';
        current = tree_entry(tree_node(current)->parent, fsnode_t);
    } while (current && current != root);

    begin++; // remove leading '/'

    const char *result = duplicate_string(begin, pool + MOS_PATH_MAX_LENGTH - begin);
    kfree(pool);
    return result;
}

void path_treeop_decrement_refcount(const tree_node_t *node)
{
    fsnode_t *path = tree_entry(node, fsnode_t);
    mos_debug(fs, "decreasing refcount of path '%s'", path->name);
    refcount_dec(&path->refcount);
}

void path_treeop_increment_refcount(const tree_node_t *node)
{
    fsnode_t *path = tree_entry(node, fsnode_t);
    mos_debug(fs, "incrementing refcount of path '%s'", path->name);
    refcount_inc(&path->refcount);
}
