// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/vfs_types.h"
#include "mos/filesystem/vfs_utils.h"
#include "mos/mm/slab.h"
#include "mos/setup.h"

#include <mos/filesystem/dentry.h>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.h>
#include <mos/lib/structures/hashmap.h>
#include <mos/lib/structures/hashmap_common.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/structures/tree.h>
#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/process.h>
#include <mos/tasks/task_types.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

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

static void mountpoint_map_init(void)
{
    hashmap_init(&vfs_mountpoint_map, VFS_MOUNTPOINT_MAP_SIZE, dentry_hash, hashmap_simple_key_compare);
}
MOS_INIT(PRE_VFS, mountpoint_map_init);
// END: Mountpoint Hashmap

list_head vfs_mountpoint_list = LIST_HEAD_INIT(vfs_mountpoint_list);

static mount_t *dentry_get_mount(const dentry_t *dentry)
{
    if (!dentry->is_mountpoint)
    {
        mos_warn("dentry is not a mountpoint");
        return NULL;
    }

    mount_t *mount = hashmap_get(&vfs_mountpoint_map, (ptr_t) dentry);
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
 * @brief Given a mounted root dentry, return the mountpoint dentry that points to it
 *
 * @param dentry The mounted root dentry
 * @return dentry_t* The mountpoint dentry
 */
static dentry_t *dentry_root_get_mountpoint(dentry_t *dentry)
{
    MOS_ASSERT(dentry);
    MOS_ASSERT_X(dentry->name == NULL, "mounted root should not have a name");

    if (dentry == root_dentry)
        return dentry; // the root dentry is its own mountpoint

    dentry_t *parent = dentry_parent(dentry);
    MOS_ASSERT(parent);

    tree_foreach_child(dentry_t, child, parent)
    {
        if (child->is_mountpoint)
        {
            mount_t *mount = dentry_get_mount(child);
            if (mount->root == dentry)
                return child;
        }
    }

    return NULL; // not found, possibly just have been unmounted
}

/**
 * @brief Decrease the refcount of ONE SINGLE dentry, including (if it's a mountpoint) the mountpoint dentry
 *
 * @param dentry The dentry to decrease the refcount of
 * @return true if the refcount was decreased, false if the refcount was already 0
 */
__nodiscard bool dentry_unref_one(dentry_t *dentry)
{
    if (dentry == NULL)
        return false;

    if (dentry->refcount == 0)
    {
        mos_warn("dentry refcount is already 0");
        return false;
    }

    dentry->refcount--;
    mos_debug(dcache_ref, "dentry %p '%s' decreased refcount to %zu", (void *) dentry, dentry_name(dentry), dentry->refcount);

    if (dentry->name == NULL && dentry != root_dentry)
    {
        dentry_t *mountpoint = dentry_root_get_mountpoint(dentry);
        if (!mountpoint)
            goto done;
        mountpoint->refcount--;
        mos_debug(dcache_ref, "  mountpoint %p '%s' decreased mountpoint refcount to %zu", (void *) mountpoint, dentry_name(mountpoint), mountpoint->refcount);
    }
done:
    return true;
}

void dentry_try_release(dentry_t *dentry)
{
    MOS_ASSERT(dentry->refcount == 0);

    const bool can_release = dentry->inode == NULL && list_is_empty(&tree_node(dentry)->children);
    if (can_release)
    {
        list_remove(&dentry->tree_node);
        if (dentry->name)
            kfree(dentry->name);
        kfree(dentry);
    }
}

// The two functions below have circular dependencies, so we need to forward declare them
// Both of them return a referenced dentry, no need to refcount them again
static dentry_t *dentry_resolve_handle_last_segment(dentry_t *parent, char *leaf, lastseg_resolve_flags_t flags, bool *symlink_resolved);
static dentry_t *dentry_resolve_follow_symlink(dentry_t *dentry, lastseg_resolve_flags_t flags);

/**
 * @brief Lookup the parent directory of a given path, and return the last segment of the path in last_seg_out
 *
 * @param base_dir A directory to start the lookup from
 * @param root_dir The root directory of the filesystem, the lookup will not go above this directory
 * @param original_path The path to lookup
 * @param last_seg_out The last segment of the path will be returned in this parameter, the caller is responsible for freeing it
 * @return dentry_t* The parent directory of the path, or NULL if the path is invalid, the dentry will be referenced
 */
static dentry_t *dentry_lookup_parent(dentry_t *base_dir, dentry_t *root_dir, const char *original_path, char **last_seg_out)
{
    mos_debug(dcache, "lookup parent of '%s'", original_path);
    MOS_ASSERT_X(base_dir && root_dir && original_path, "Invalid VFS lookup parameters");
    if (last_seg_out != NULL)
        *last_seg_out = NULL;

    dentry_t *parent_ref = statement_expr(dentry_t *, {
        dentry_t *__parent = path_is_absolute(original_path) ? root_dir : base_dir;
        if (__parent->is_mountpoint)
            __parent = dentry_get_mount(__parent)->root; // if it's a mountpoint, jump to mounted filesystem
        __parent = dentry_ref_up_to(__parent, root_dir);
        retval = __parent;
    });

    char *saveptr = NULL;
    char *path = strdup(original_path);
    const char *current_seg = strtok_r(path, PATH_DELIM_STR, &saveptr);
    if (unlikely(current_seg == NULL))
    {
        // this only happens if the path is empty, or contains only slashes
        // in which case we return the base directory
        kfree(path);
        if (last_seg_out != NULL)
            *last_seg_out = NULL;
        return parent_ref;
    }

    while (true)
    {
        mos_debug(dcache, "lookup parent: current segment '%s'", current_seg);
        const char *const next = strtok_r(NULL, PATH_DELIM_STR, &saveptr);
        if (next == NULL)
        {
            if (parent_ref->inode->type == FILE_TYPE_SYMLINK)
            {
                // this is the real interesting dir
                dentry_t *const parent_real_ref = dentry_resolve_follow_symlink(parent_ref, RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_DIR);
                dentry_unref(parent_ref);
                if (unlikely(parent_real_ref == NULL))
                    return ERR_PTR(-ENOENT); // the symlink target does not exist
                parent_ref = parent_real_ref;
            }

            // "current_seg" is the last segment of the path
            if (last_seg_out != NULL)
            {
                const bool ends_with_slash = original_path[strlen(original_path) - 1] == PATH_DELIM;
                char *tmp = kmalloc(strlen(current_seg) + 2); // +2 for the null terminator and the slash
                strcpy(tmp, current_seg);
                if (ends_with_slash)
                    strcat(tmp, PATH_DELIM_STR);
                *last_seg_out = tmp;
            }

            kfree(path);
            return parent_ref;
        }

        if (strncmp(current_seg, ".", 2) == 0 || strcmp(current_seg, "./") == 0)
        {
            current_seg = next;
            continue;
        }

        if (strncmp(current_seg, "..", 3) == 0 || strcmp(current_seg, "../") == 0)
        {
            if (parent_ref == root_dir)
            {
                // we can't go above the root directory
                current_seg = next;
                continue;
            }

            dentry_t *parent = dentry_parent(parent_ref);
            MOS_ASSERT(dentry_unref_one(parent_ref)); // don't recurse up to the root
            parent_ref = parent;

            // if the parent is a mountpoint, we need to jump to the mountpoint's parent
            // and then jump to the mountpoint's parent's parent
            // already referenced when we jumped to the mountpoint
            if (parent_ref->is_mountpoint)
                parent_ref = dentry_root_get_mountpoint(parent);

            current_seg = next;
            continue;
        }

        dentry_t *const child_ref = dentry_get_child(parent_ref, current_seg);
        if (child_ref->inode == NULL)
        {
            *last_seg_out = NULL;
            kfree(path);
            dentry_try_release(child_ref);
            dentry_unref(parent_ref);
            return ERR_PTR(-ENOENT);
        }

        if (child_ref->is_mountpoint)
        {
            mos_debug(dcache, "jumping to mountpoint %s", child_ref->name);
            parent_ref = dentry_get_mount(child_ref)->root; // if it's a mountpoint, jump to the tree of mounted filesystem instead

            // refcount the mounted filesystem root
            dentry_ref(parent_ref);
        }
        else
        {
            parent_ref = child_ref;
        }

        current_seg = next;
    }

    MOS_UNREACHABLE();
}

static dentry_t *dentry_resolve_follow_symlink(dentry_t *symlink_dentry, lastseg_resolve_flags_t flags)
{
    MOS_ASSERT_X(symlink_dentry != NULL && symlink_dentry->inode != NULL, "check before calling this function!");
    MOS_ASSERT_X(symlink_dentry->inode->type == FILE_TYPE_SYMLINK, "check before calling this function!");

    if (!symlink_dentry->inode->ops || !symlink_dentry->inode->ops->readlink)
        mos_panic("inode does not support readlink (symlink) operation, but it's a symlink!");

    char *const target = kmalloc(MOS_PATH_MAX_LENGTH);
    const size_t read = symlink_dentry->inode->ops->readlink(symlink_dentry, target, MOS_PATH_MAX_LENGTH);
    if (read == 0)
    {
        mos_warn("symlink is empty");
        return ERR_PTR(-ENOENT); // symlink is empty
    }

    if (read == MOS_PATH_MAX_LENGTH)
    {
        mos_warn("symlink is too long");
        return ERR_PTR(-ENAMETOOLONG); // symlink is too long
    }

    target[read] = '\0'; // ensure null termination

    mos_debug(dcache, "symlink target: %s", target);

    char *last_segment = NULL;
    dentry_t *parent_ref = dentry_lookup_parent(dentry_parent(symlink_dentry), root_dentry, target, &last_segment);
    kfree(target);
    if (IS_ERR(parent_ref))
    {
        mos_warn("symlink target does not exist");
        return parent_ref;
    }

    // it's possibly that the symlink target is also a symlink, this will be handled recursively
    bool symlink = false;
    dentry_t *child_ref = dentry_resolve_handle_last_segment(parent_ref, last_segment, flags, &symlink);
    kfree(last_segment);

    // if symlink is true, we need to unref the parent_ref dentry as it's irrelevant now
    if (IS_ERR(child_ref) || symlink)
        dentry_unref(parent_ref);

    return child_ref; // the real dentry, or an error code
}

static dentry_t *dentry_resolve_handle_last_segment(dentry_t *parent, char *leaf, lastseg_resolve_flags_t flags, bool *is_symlink)
{
    MOS_ASSERT(parent != NULL && leaf != NULL);
    *is_symlink = false;

    mos_debug(dcache, "resolving last segment: '%s'", leaf);
    const bool ends_with_slash = leaf[strlen(leaf) - 1] == PATH_DELIM;
    if (ends_with_slash)
        leaf[strlen(leaf) - 1] = '\0'; // remove the trailing slash

    if (unlikely(ends_with_slash && !(flags & RESOLVE_EXPECT_DIR)))
    {
        mos_warn("RESOLVE_EXPECT_DIR isn't set, but the provided path ends with a slash");
        return ERR_PTR(-EINVAL);
    }

    if (strncmp(leaf, ".", 2) == 0 || strcmp(leaf, "./") == 0)
        return parent;

    if (strncmp(leaf, "..", 3) == 0 || strcmp(leaf, "../") == 0)
    {
        if (parent == root_dentry)
            return parent;

        dentry_t *parent_parent = dentry_parent(parent);
        MOS_ASSERT(dentry_unref_one(parent)); // don't recursively unref all the way to the root

        if (parent_parent->is_mountpoint)
            parent_parent = dentry_root_get_mountpoint(parent_parent);
        return parent_parent;
    }

    dentry_t *child_ref = dentry_get_child(parent, leaf); // now we have a reference to the child

    if (unlikely(child_ref->inode == NULL))
    {
        if (flags & RESOLVE_EXPECT_NONEXIST)
        {
            // do not use dentry_ref, because it checks for an inode
            child_ref->refcount++;
            return child_ref;
        }

        mos_debug(dcache, "file does not exist");
        dentry_try_release(child_ref);
        return ERR_PTR(-ENOENT);
    }

    MOS_ASSERT(child_ref->refcount > 0); // dentry_get_child may return a negative dentry, which is handled above, otherwise we should have a reference on it

    if (flags & RESOLVE_EXPECT_NONEXIST && !(flags & RESOLVE_EXPECT_EXIST))
    {
        mos_warn("file already exists");
        dentry_unref(child_ref);
        return ERR_PTR(-EEXIST);
    }

    if (child_ref->inode->type == FILE_TYPE_SYMLINK)
    {
        if (flags & RESOLVE_SYMLINK_NOFOLLOW)
        {
            mos_debug(dcache, "not resolving symlink");
            return child_ref;
        }

        mos_debug(dcache, "resolving symlink: %s", leaf);
        dentry_t *const symlink_target_ref = dentry_resolve_follow_symlink(child_ref, flags);
        // we don't need the symlink node anymore
        MOS_ASSERT(dentry_unref_one(child_ref));
        *is_symlink = symlink_target_ref != NULL;
        return symlink_target_ref;
    }

    if (child_ref->inode->type == FILE_TYPE_DIRECTORY)
    {
        if (!(flags & RESOLVE_EXPECT_DIR))
        {
            MOS_ASSERT(dentry_unref_one(child_ref)); // it's the caller's responsibility to unref the parent and grandparents
            return ERR_PTR(-EISDIR);
        }

        if (child_ref->is_mountpoint)
            child_ref = dentry_ref(dentry_get_mount(child_ref)->root);
    }
    else
    {
        if (!(flags & RESOLVE_EXPECT_FILE))
        {
            MOS_ASSERT(dentry_unref_one(child_ref)); // it's the caller's responsibility to unref the parent and grandparents
            return ERR_PTR(-ENOTDIR);
        }
    }

    return child_ref;
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

dentry_t *dentry_ref(dentry_t *dentry)
{
    MOS_ASSERT(dentry);
    MOS_ASSERT(dentry->inode); // it's the root, or it has an inode (not a negative dentry)
    dentry->refcount++;
    mos_debug(dcache_ref, "dentry %p '%s' increased refcount to %zu", (void *) dentry, dentry_name(dentry), dentry->refcount);
    return dentry;
}

dentry_t *dentry_ref_up_to(dentry_t *dentry, dentry_t *root)
{
    mos_debug(dcache_ref, "dentry_ref_up_to(%p '%s', %p '%s')", (void *) dentry, dentry_name(dentry), (void *) root, dentry_name(root));
    for (dentry_t *cur = dentry; cur != root; cur = dentry_parent(cur))
    {
        dentry_ref(cur);
        if (cur->name == NULL)
        {
            cur = dentry_root_get_mountpoint(cur);
            dentry_ref(cur);
        }
    }

    dentry_ref(root); // it wasn't refcounted in the loop

    mos_debug(dcache_ref, "...done");
    return dentry;
}

void dentry_dump_refstat(const dentry_t *dentry, dump_refstat_receiver_t *receiver, void *receiver_data)
{
    if (dentry == NULL)
        return;
    static int depth = 0;

    receiver(depth, dentry, false, receiver_data);

    if (dentry->is_mountpoint)
    {
        dentry = dentry_get_mount(dentry)->root;
        receiver(depth, dentry, true, receiver_data);
    }

    depth++;
    tree_foreach_child(dentry_t, child, dentry)
    {
        dentry_dump_refstat(child, receiver, receiver_data);
    }
    depth--;
}

void dentry_check_refstat(const dentry_t *dentry)
{
    size_t expected_refcount = 0;

    if (dentry != root_dentry)
    {
        if (dentry->is_mountpoint)
            expected_refcount++; // the mountpoint itself

        if (dentry->name == NULL)
            expected_refcount++; // the mounted root dentry
    }
    else
    {
        expected_refcount++; // the root dentry should only has one reference
    }

    tree_foreach_child(dentry_t, child, dentry)
    {
        expected_refcount += child->refcount;
    }

    if (dentry->refcount < expected_refcount)
    {
        mos_warn("dentry %p refcount %zu is less than expected refcount %zu", (void *) dentry, dentry->refcount, expected_refcount);
        tree_foreach_child(dentry_t, child, dentry)
        {
            pr_warn("  child %p '%s' has %zu references", (void *) child, dentry_name(child), child->refcount);
        }
        mos_panic("don't know how to handle this");
    }
    else if (dentry->refcount - expected_refcount)
    {
        mos_debug(dcache_ref, "  dentry %p '%s' has %zu direct references", (void *) dentry, dentry_name(dentry), dentry->refcount - expected_refcount);
    }
}

void dentry_unref(dentry_t *dentry)
{
    if (!dentry_unref_one(dentry))
        return;

    dentry_check_refstat(dentry);
    dentry_unref(dentry_parent(dentry));

    if (dentry->refcount == 0)
        dentry_try_release(dentry);
}

dentry_t *dentry_from_fd(fd_t fd)
{
    if (fd == FD_CWD)
    {
        if (current_thread)
            return current_process->working_directory;
        else
            return root_dentry; // no current process, so cwd is always root
    }

    // sanity check: fd != FD_CWD, no current process
    MOS_ASSERT(current_thread);

    io_t *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return ERR_PTR(-EBADF);

    file_t *file = container_of(io, file_t, io);
    return file->dentry;
}

dentry_t *dentry_get_child(dentry_t *parent, const char *name)
{
    if (unlikely(parent == NULL))
        return NULL;

    mos_debug(dcache, "looking for dentry '%s' in '%s'", name, dentry_name(parent));

    dentry_t *dentry = NULL;

    // firstly check if it's in the cache
    tree_foreach_child(dentry_t, c, parent)
    {
        if (strcmp(c->name, name) == 0)
        {
            if (c->inode != NULL)
            {
                mos_debug(dcache, "found dentry '%s' in cache", name);
                return dentry_ref(c);
            }
            else
            {
                mos_debug(dcache, "found dentry '%s' in cache, but it's not backed by an inode, try looking up", name);
                dentry = c;
                break;
            }
        }
    }

    // regardless of whether we found it in the real FS or not, we leave it in the cache
    if (dentry == NULL)
        dentry = dentry_create(parent->superblock, parent, name);

    if (dentry->inode)
        return dentry_ref(dentry);

    // not in the cache, try to find it in the filesystem
    if (parent->inode == NULL || parent->inode->ops == NULL || parent->inode->ops->lookup == NULL)
    {
        mos_debug(dcache, "filesystem doesn't support lookup");
        return dentry;
    }

    if (parent->inode->ops->lookup(parent->inode, dentry))
    {
        mos_debug(dcache, "dentry '%s' found in the filesystem", name);
        return dentry_ref(dentry);
    }
    else
    {
        mos_debug(dcache, "dentry '%s' not found in the filesystem", name);
        return dentry;
    }
}

dentry_t *dentry_get(dentry_t *starting_dir, dentry_t *root_dir, const char *path, lastseg_resolve_flags_t flags)
{
    if (!root_dir)
        return ERR_PTR(-ENOENT); // no root directory

    char *last_segment;
    mos_debug(dcache, "resolving path '%s'", path);
    dentry_t *const parent_ref = dentry_lookup_parent(starting_dir, root_dir, path, &last_segment);
    if (IS_ERR(parent_ref))
    {
        mos_debug(dcache, "failed to resolve parent of '%s', file not found", path);
        return parent_ref;
    }

    if (last_segment == NULL)
    {
        // path is a single "/"
        mos_debug(dcache, "path '%s' is a single '/' or is empty", path);
        MOS_ASSERT(parent_ref == starting_dir);
        if (!(flags & RESOLVE_EXPECT_DIR))
        {
            dentry_unref(parent_ref);
            return ERR_PTR(-EISDIR);
        }

        return parent_ref;
    }

    bool symlink = false;
    dentry_t *child_ref = dentry_resolve_handle_last_segment(parent_ref, last_segment, flags, &symlink);
    kfree(last_segment);
    if (IS_ERR(child_ref) || symlink)
        dentry_unref(parent_ref); // the lookup failed, or child_ref is irrelevant with the parent_ref

    return child_ref;
}

bool dentry_mount(dentry_t *mountpoint, dentry_t *root, filesystem_t *fs)
{
    MOS_ASSERT_X(root->name == NULL, "mountpoint already has a name");
    MOS_ASSERT_X(dentry_parent(root) == NULL, "mountpoint already has a parent");

    dentry_ref(root);
    tree_node(root)->parent = tree_node(dentry_parent(mountpoint));
    mountpoint->is_mountpoint = true;

    mount_t *mount = kmalloc(mount_cache);
    linked_list_init(list_node(mount));
    list_node_append(&vfs_mountpoint_list, list_node(mount));
    mount->root = root;
    mount->superblock = root->inode->superblock;
    mount->mountpoint = mountpoint;
    mount->fs = fs;

    if (hashmap_put(&vfs_mountpoint_map, (ptr_t) mountpoint, mount) != NULL)
    {
        mos_warn("failed to insert mountpoint into hashmap");
        return false;
    }

    return true;
}

// remove root from the mount tree
dentry_t *dentry_unmount(dentry_t *root)
{
    mount_t *mount = dentry_get_mount(dentry_root_get_mountpoint(root));
    if (mount == NULL)
        return NULL;

    dentry_t *mountpoint = mount->mountpoint;

    hashmap_remove(&vfs_mountpoint_map, (ptr_t) mount->mountpoint);
    list_node_remove(list_node(mount));
    kfree(mount);
    mountpoint->is_mountpoint = false;
    return mountpoint;
}

static size_t dentry_default_iterate(const dentry_t *dir, dir_iterator_state_t *state, dentry_iterator_op op)
{
    size_t written = 0;

    size_t i = DIR_ITERATOR_NTH_START;
    tree_foreach_child(dentry_t, child, dir)
    {
        if (child->inode == NULL)
            continue;

        // skip entries until we reach the nth one
        if (state->dir_nth != i++)
            continue;

        size_t w = op(state, child->inode->ino, child->name, strlen(child->name), child->inode->type);
        if (w == 0)
            break;
        written += w;
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
        dentry_t *parent = dentry_parent(dir);
        if (parent == NULL)
            parent = root_dentry;

        MOS_ASSERT(parent->inode != NULL);
        size_t w = dentry_add_dir(state, parent->inode->ino, "..", 2, FILE_TYPE_DIRECTORY);
        if (w == 0)
            return written;
        written += w;
    }

    MOS_ASSERT(dir->inode);

    // this call may not write all the entries, because the buffer may not be big enough
    if (dir->inode->ops && dir->inode->ops->iterate_dir)
        written += dir->inode->ops->iterate_dir(dir->inode, state, dentry_add_dir);
    else
        written += dentry_default_iterate(dir, state, dentry_add_dir);

    MOS_ASSERT(written <= state->buf_capacity); // we should never write more than the buffer can hold

    return written;
}

ssize_t dentry_path(dentry_t *dentry, dentry_t *root, char *buf, size_t size)
{
    if (dentry == NULL)
        return 0;

    if (size < 2)
        return -1;

    if (dentry == root)
    {
        buf[0] = '/';
        buf[1] = '\0';
        return 1;
    }

    if (dentry->name == NULL)
        dentry = dentry_root_get_mountpoint(dentry);

    char *path = strdup(dentry->name);

    for (dentry_t *current = dentry_parent(dentry); current != root; current = dentry_parent(current))
    {
        if (current->name == NULL)
            current = dentry_root_get_mountpoint(current);

        char *newpath = kmalloc(strlen(current->name) + 1 + strlen(path) + 1);
        strcpy(newpath, current->name);
        strcat(newpath, "/");
        strcat(newpath, path);
        kfree(path);
        path = newpath;
    }

    if (strlen(path) + 1 > size)
        return -1;

    const size_t real_size = snprintf(buf, size, "/%s", path);
    kfree(path);

    return real_size;
}
