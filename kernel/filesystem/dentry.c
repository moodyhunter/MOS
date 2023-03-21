// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/filesystem/dentry.h>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.h>
#include <mos/lib/structures/hashmap.h>
#include <mos/lib/structures/hashmap_common.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/structures/tree.h>
#include <mos/mm/kmalloc.h>
#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/task_types.h>
#include <string.h>

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

static hash_t dentry_hash(uintn key)
{
    return (hash_t){ .hash = key };
}
// END: Mountpoint Hashmap

static mount_t *dentry_find_mount(dentry_t *dentry)
{
    if (!dentry->is_mountpoint)
    {
        mos_warn("dentry is not a mountpoint");
        return NULL;
    }

    mount_t *mount = hashmap_get(&vfs_mountpoint_map, (uintptr_t) dentry);
    if (mount == NULL)
    {
        mos_warn("mountpoint not found");
        return NULL;
    }

    // otherwise the mountpoint must match the dentry
    MOS_ASSERT(mount->mountpoint == dentry);
    return mount;
}

/**
 * @brief Lookup the parent directory of a given path, and return the last segment of the path in last_seg_out
 *
 * @param base_dir A directory to start the lookup from
 * @param root_dir The root directory of the filesystem, the lookup will not go above this directory
 * @param original_path The path to lookup
 * @param last_seg_out The last segment of the path will be returned in this parameter, the caller is responsible for freeing it
 * @return dentry_t* The parent directory of the path, or NULL if the path is invalid
 */
static dentry_t *dentry_lookup_parent(dentry_t *base_dir, dentry_t *root_dir, const char *original_path, char **last_seg_out)
{
    MOS_ASSERT_X(base_dir && root_dir && original_path, "Invalid VFS lookup parameters");
    char *saveptr;
    char *path = strdup(original_path);

    mos_debug(vfs, "lookup parent of %s", path);

    char *current_seg = strtok_r(path, PATH_DELIM_STR, &saveptr);
    dentry_t *current_dir = path_is_absolute(path) ? root_dir : base_dir;

    if (current_dir->is_mountpoint)
        current_dir = dentry_find_mount(current_dir)->root; // if it's a mountpoint, jump to the tree of mounted filesystem instead

    if (unlikely(current_seg == NULL))
    {
        // this only happens if the path is empty, or contains only slashes
        // in which case we return the base directory
        kfree(path);
        if (last_seg_out != NULL)
            *last_seg_out = NULL;
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

        current_dir = child;
        current_seg = next;

        if (current_dir->is_mountpoint)
        {
            mos_debug(vfs, "jumping to mountpoint %s", current_dir->name);
            current_dir = dentry_find_mount(current_dir)->root; // if it's a mountpoint, jump to the tree of mounted filesystem instead
        }
    }

    MOS_UNREACHABLE();
}

static dentry_t *dentry_resolve_handle_last_segment(dentry_t *parent, char *leaf, lastseg_resolve_flags_t flags);
static dentry_t *dentry_resolve_follow_symlink(dentry_t *dentry, lastseg_resolve_flags_t flags);

static dentry_t *dentry_resolve_follow_symlink(dentry_t *dentry, lastseg_resolve_flags_t flags)
{
    MOS_ASSERT_X(dentry != NULL && dentry->inode != NULL, "check before calling this function!");
    MOS_ASSERT_X(dentry->inode->stat.type == FILE_TYPE_SYMLINK, "check before calling this function!");

    char *const target = kmalloc(MOS_PATH_MAX_LENGTH);
    size_t read = dentry->inode->ops->readlink(dentry, target, MOS_PATH_MAX_LENGTH);
    if (read == 0)
    {
        mos_warn("symlink is empty");
        return NULL;
    }

    if (read == MOS_PATH_MAX_LENGTH)
    {
        mos_warn("symlink is too long");
        return NULL;
    }

    target[read] = '\0'; // ensure null termination

    mos_debug(vfs, "symlink target: %s", target);

    char *last_segment;
    dentry_t *parent = dentry_lookup_parent(tree_parent(dentry, dentry_t), root_dentry, target, &last_segment);
    if (parent == NULL)
    {
        mos_warn("symlink target does not exist");
        return NULL;
    }

    // it's possibly that the symlink target is also a symlink, this will be handled recursively
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

    mos_debug(vfs, "resolving last segment: %s", leaf);
    const bool ends_with_slash = leaf[strlen(leaf) - 1] == PATH_DELIM;
    if (ends_with_slash)
        leaf[strlen(leaf) - 1] = '\0'; // remove the trailing slash

    if (unlikely(ends_with_slash && !(flags & RESOLVE_EXPECT_DIR)))
    {
        mos_warn("RESOLVE_EXPECT_DIR isn't set, but the provided path ends with a slash");
        return NULL;
    }

    dentry_t *child = dentry_get_child(parent, leaf); // now we have a reference to the child

    if (unlikely(child == NULL))
    {
        if (!(flags & RESOLVE_EXPECT_NONEXIST))
        {
            mos_warn("file does not exist, didn't expect it");
            return NULL;
        }

        if (flags & RESOLVE_WILL_CREATE)
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

    if (child->inode->stat.type == FILE_TYPE_SYMLINK)
    {
        if (flags & RESOLVE_SYMLINK_NOFOLLOW)
        {
            mos_debug(vfs, "not resolving symlink");
            return child;
        }

        mos_debug(vfs, "resolving symlink: %s", leaf);
        dentry_t *symlink_target = dentry_resolve_follow_symlink(child, flags);
        if (unlikely(symlink_target == NULL))
        {
            mos_warn("failed to resolve symlink");
            return NULL;
        }

        return symlink_target;
    }

    if (child->inode->stat.type == FILE_TYPE_DIRECTORY)
    {
        if (child->is_mountpoint)
            child = dentry_find_mount(child)->root;

        if (!(flags & RESOLVE_EXPECT_DIR))
        {
            mos_warn("got a directory, didn't expect it");
            return NULL;
        }
    }
    else
    {
        if (!(flags & RESOLVE_EXPECT_FILE))
        {
            mos_warn("got a file, didn't expect it");
            return NULL;
        }
    }

    return child;
}

static size_t dentry_add_dir(dir_iterator_state_t *state, u64 ino, const char *name, size_t name_len, file_type_t type)
{
    const size_t this_record_size = sizeof(dir_entry_t) + name_len + 1; // + 1 for null terminator

    if (state->buf_capacity - state->buf_written < this_record_size)
        return 0; // not enough space

    dir_entry_t *entry = (dir_entry_t *) (state->buf + state->buf_written);
    entry->ino = ino;
    entry->next_offset = this_record_size;
    entry->type = type;
    entry->name_len = name_len;

    strcpy(entry->name, name);
    entry->name[entry->name_len] = '\0'; // ensure null termination

    state->dir_nth++;
    state->buf_written += this_record_size;
    return this_record_size;
}

void dentry_cache_init(void)
{
    pr_info2("initializing dentry cache...");
    hashmap_init(&vfs_mountpoint_map, VFS_MOUNTPOINT_MAP_SIZE, dentry_hash, hashmap_simple_key_compare);
}

void dentry_unref(dentry_t *dentry)
{
    if (dentry == NULL)
        return;

    if (dentry->refcount == 0)
    {
        mos_warn("dentry refcount is already 0");
        return;
    }

    dentry->refcount--;
    if (dentry->refcount == 0)
    {
        // remove from parent's children list
        list_remove(&dentry->tree_node);

        if (dentry->name)
            kfree(dentry->name);
        kfree(dentry);
    }
}

dentry_t *dentry_create(dentry_t *parent, const char *name)
{
    dentry_t *dentry = kzalloc(sizeof(dentry_t));
    linked_list_init(&tree_node(dentry)->children);
    linked_list_init(&tree_node(dentry)->list_node);

    if (name)
        dentry->name = strdup(name);

    if (parent)
    {
        tree_add_child(tree_node(parent), tree_node(dentry));
        dentry->superblock = parent->superblock;
    }
    return dentry_ref(dentry);
}

dentry_t *dentry_from_fd(fd_t fd)
{
    if (fd == FD_CWD)
        return current_process->working_directory;
    else
        MOS_UNIMPLEMENTED("dentry_from_fd for a non-FD_CWD fd");
}

dentry_t *dentry_get_child(dentry_t *parent, const char *name)
{
    if (unlikely(parent == NULL))
        return NULL;

    // firstly check if it's in the cache
    tree_foreach_child(dentry_t, c, parent)
    {
        if (strcmp(c->name, name) == 0)
        {
            mos_debug(vfs, "found dentry '%s' in cache", name);
            return dentry_ref(c);
        }
    }

    // not in the cache, try to find it in the filesystem
    if (parent->inode == NULL)
        return NULL;

    dentry_t *target = dentry_create(parent, name);

    bool result = false;

    if (parent->inode->ops->lookup)
        result = parent->inode->ops->lookup(parent->inode, target);

    if (unlikely(!result))
    {
        dentry_unref(target);
        return NULL;
    }

    return target;
}

dentry_t *dentry_get(dentry_t *base_dir, dentry_t *root_dir, const char *path, lastseg_resolve_flags_t flags)
{
    char *last_segment;
    mos_debug(vfs, "resolving path '%s'", path);
    dentry_t *parent = dentry_lookup_parent(base_dir, root_dir, path, &last_segment);
    if (parent == NULL)
    {
        mos_warn("file does not exist");
        return NULL;
    }

    if (last_segment == NULL)
    {
        // path is a single "/"
        mos_debug(vfs, "path '%s' is a single '/'", path);
        MOS_ASSERT(parent == root_dir);
        if (!(flags & RESOLVE_EXPECT_DIR))
        {
            mos_warn("RESOLVE_EXPECT_DIR flag not set, but path is the root directory");
            return NULL;
        }

        return dentry_ref(parent);
    }

    return dentry_resolve_handle_last_segment(parent, last_segment, flags);
}

bool dentry_mount(dentry_t *mountpoint, dentry_t *root, filesystem_t *fs)
{
    mount_t *mount = kzalloc(sizeof(mount_t));
    mount->root = root;
    mount->superblock = root->inode->superblock;
    mount->mountpoint = mountpoint;
    mount->fs = fs;
    mountpoint->is_mountpoint = true;

    if (hashmap_put(&vfs_mountpoint_map, (uintptr_t) mountpoint, mount) != NULL)
    {
        mos_warn("failed to insert mountpoint into hashmap");
        return false;
    }

    return true;
}

static size_t dentry_default_iterate(const dentry_t *dir, dir_iterator_state_t *state, dentry_iterator_op op)
{
    size_t written = 0;

    size_t i = DIR_ITERATOR_NTH_START;
    tree_foreach_child(dentry_t, child, dir)
    {
        if (state->dir_nth == i)
        {
            size_t w = op(state, child->inode->ino, child->name, strlen(child->name), child->inode->stat.type);
            if (w == 0)
                return written;
            written += w;
        }

        i++;
    }

    return written;
}

size_t dentry_list(dentry_t *dir, dir_iterator_state_t *state)
{
    size_t written = 0;

    if (state->dir_nth == 0)
    {
        inode_t *inode = dir->inode;
        size_t w;
        w = dentry_add_dir(state, inode->ino, ".", 1, FILE_TYPE_DIRECTORY);
        if (w == 0)
            return written;
        written += w;
    }

    if (state->dir_nth == 1)
    {
        dentry_t *parent = tree_parent(dir, dentry_t);
        if (parent == NULL)
            parent = root_dentry;

        MOS_ASSERT(parent->inode != NULL);
        size_t w = dentry_add_dir(state, parent->inode->ino, "..", 2, FILE_TYPE_DIRECTORY);
        if (w == 0)
            return written;
        written += w;
    }

    MOS_ASSERT(dir->inode && dir->inode->ops);

    // this call may not write all the entries, because the buffer may not be big enough
    if (dir->inode->ops && dir->inode->ops->iterate_dir)
        written += dir->inode->ops->iterate_dir(dir->inode, state, dentry_add_dir);
    else
        written += dentry_default_iterate(dir, state, dentry_add_dir);

    MOS_ASSERT(written <= state->buf_capacity); // we should never write more than the buffer can hold

    return written;
}
