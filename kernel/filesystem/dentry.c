// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/dentry.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "lib/structures/hashmap_common.h"
#include "lib/structures/list.h"
#include "lib/structures/tree.h"
#include "mos/filesystem/fs_types.h"
#include "mos/filesystem/mount.h"
#include "mos/mm/kmalloc.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/task_types.h"

// A path in its string form is composed of "segments" separated
// by a slash "/", a path may:
//
// - begin with a slash, indicating that's an absolute path
// - begin without a slash, indicating that's a relative path
//   (relative to the current working directory (FD_CWD))
//
// A path may end with a slash, indicating that the caller expects
// the path to be a directory

// BEGIN: Mountpoint Hashmap
#define VFS_MOUNTPOINT_MAP_SIZE 256
static hashmap_t vfs_mountpoint_map = { 0 }; // dentry_t -> mount_t

static hash_t dentry_hash(const void *key)
{
    return (hash_t){ .hash = (uintptr_t) (dentry_t *) key };
}
// END: Mountpoint Hashmap

static dentry_t *root_dentry;

mount_t *dentry_find_mount(dentry_t *dentry)
{
    if (dentry->inode == NULL)
    {
        mos_warn("dentry provided doesn't have a backing inode"); // ??? is this possible?
        return NULL;
    }

    if (dentry->inode->stat.type != FILE_TYPE_DIRECTORY)
    {
        mos_warn("mountpoint is not a directory");
        return NULL;
    }

    if (!dentry->is_mountpoint)
    {
        mos_warn("dentry is not a mountpoint");
        return NULL;
    }

    mount_t *mount = hashmap_get(&vfs_mountpoint_map, dentry);
    if (mount == NULL)
    {
        mos_warn("mountpoint not found");
        return NULL;
    }

    // otherwise the mountpoint must match the dentry
    MOS_ASSERT(mount->mountpoint == dentry);
    return mount;
}

void dentry_init(void)
{
    hashmap_init(&vfs_mountpoint_map, VFS_MOUNTPOINT_MAP_SIZE, dentry_hash, hashmap_simple_key_compare);
}

#define FD_CWD -69

dentry_t *dentry_from_fd(fd_t fd)
{
    if (fd == FD_CWD)
        return current_process->working_directory;
    else
        MOS_UNIMPLEMENTED("dentry_from_fd for a non-FD_CWD fd");
}

bool path_is_absolute(const char *path)
{
    if (strlen(path) == 0)
        return false; // empty path is not absolute

    return path[0] == '/';
}

dentry_t *dentry_create(dentry_t *parent, const char *name)
{
    dentry_t *dentry = kzalloc(sizeof(dentry_t));
    dentry->name = strdup(name);
    tree_add_child(tree_node(parent), tree_node(dentry));
    return dentry_ref(dentry);
}

dentry_t *dentry_get_child(const dentry_t *parent, const char *name)
{
    if (parent == NULL)
        return NULL;

    tree_foreach_child(dentry_t, c, parent)
    {
        if (strcmp(c->name, name) == 0)
            return dentry_ref(c);
    }

    return NULL;
}

static dentry_t *dentry_lookup_parent(dentry_t *base_dir, dentry_t *root_dir, const char *original_path, char **last_seg_out)
{
    char *saveptr;
    char *path = strdup(original_path);

    char *current_seg = strtok_r(path, PATH_DELIM_STR, &saveptr);
    dentry_t *current_dir = path_is_absolute(path) ? root_dir : base_dir;

    if (unlikely(current_seg == NULL))
    {
        // this only happens if the path is empty, or contains only slashes
        // in which case we return the base directory
        kfree(path);
        return current_dir;
    }

    while (true)
    {
        char *next = strtok_r(NULL, PATH_DELIM_STR, &saveptr);
        if (next == NULL)
        {
            // "current_seg" is the last segment of the path
            // TODO: handle . and .. final segments

            if (last_seg_out != NULL)
            {
                const bool ends_with_slash = original_path[strlen(original_path) - 1] == PATH_DELIM;
                char *tmp = kzalloc(strlen(current_seg) + 2); // +2 for the null terminator and the slash
                strcpy(tmp, current_seg);
                if (ends_with_slash)
                    strcat(tmp, PATH_DELIM_STR);
                *last_seg_out = tmp;
            }

            kfree(path);
            return current_dir;
        }

        // if (name_len == 0)
        //     return NULL;

        // if (name_len == 1 && name[0] == '.')
        //     return (dentry_t *) parent;

        // if (name_len == 2 && name[0] == '.' && name[1] == '.')
        //     return tree_entry(tree_node(parent)->parent, dentry_t);

        dentry_t *child = dentry_get_child(current_dir, current_seg);
        if (child == NULL)
        {
            *last_seg_out = NULL;
            kfree(path);
            return NULL; // non-existent file?
        }

        base_dir = child;
        current_seg = next;
    }

    MOS_UNREACHABLE();
}

static dentry_t *dentry_resolve_handle_last_segment(dentry_t *parent, char *leaf, lastseg_resolve_flags_t flags);
static dentry_t *dentry_resolve_follow_symlink(dentry_t *dentry, lastseg_resolve_flags_t flags);

static dentry_t *dentry_resolve_follow_symlink(dentry_t *dentry, lastseg_resolve_flags_t flags)
{
    MOS_ASSERT_X(dentry != NULL && dentry->inode != NULL, "check before calling this function!");
    MOS_ASSERT_X(dentry->inode->stat.type == FILE_TYPE_SYMLINK, "check before calling this function!");

    char *const target = kmalloc(PATH_MAX);
    size_t read = dentry->inode->ops->readlink(dentry, target, PATH_MAX);
    if (read == 0)
    {
        mos_warn("symlink is empty");
        return NULL;
    }

    if (read == PATH_MAX)
    {
        mos_warn("symlink is too long");
        return NULL;
    }

    target[read] = '\0'; // ensure null termination

    char *last_segment;
    dentry_t *base = current_process->working_directory;
    dentry_t *parent = dentry_lookup_parent(base, root_dentry, target, &last_segment);
    if (parent == NULL)
    {
        mos_warn("symlink target does not exist");
        return NULL;
    }

    dentry_t *child = dentry_resolve_handle_last_segment(parent, last_segment, flags);
    if (child == NULL)
    {
        mos_warn("symlink target does not exist");
        return NULL;
    }

    return child;
}

static dentry_t *dentry_resolve_handle_last_segment(dentry_t *parent, char *leaf, lastseg_resolve_flags_t flags)
{
    MOS_ASSERT(parent != NULL && leaf != NULL);

    dentry_t *child = dentry_get_child(parent, leaf); // now we have a reference to the child

    if (unlikely(child == NULL))
    {
        if (flags & LASTSEG_ALLOW_NONEXISTENT)
        {
            child = dentry_create(parent, leaf);
            if (unlikely(child == NULL))
            {
                mos_warn("failed to create dentry");
                return NULL;
            }

            return child;
        }

        mos_warn("file does not exist");
        return NULL;
    }

    if (child->inode == NULL)
    {
        mos_warn("file does not exist?");
        mos_panic("don't know how to handle this");
        return NULL;
    }

    if (flags & LASTSEG_FOLLOW_SYMLINK)
    {
        dentry_t *symlink_target = dentry_resolve_follow_symlink(child, flags);
    }

    return NULL;
}

bool dentry_mount(dentry_t *mountpoint, dentry_t *root)
{
    MOS_UNUSED(mountpoint);
    MOS_UNUSED(root);

    return false;
}
