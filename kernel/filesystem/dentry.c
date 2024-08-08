// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/dentry.h"

#include "mos/assert.h"
#include "mos/filesystem/inode.h"
#include "mos/filesystem/mount.h"
#include "mos/filesystem/vfs.h"
#include "mos/filesystem/vfs_types.h"
#include "mos/filesystem/vfs_utils.h"
#include "mos/io/io.h"
#include "mos/lib/sync/spinlock.h"
#include "mos/syslog/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_types.h"

#include <mos/filesystem/fs_types.h>
#include <mos/lib/structures/hashmap_common.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

// A path in its string form is composed of "segments" separated
// by a slash "/", a path may:
//
// - begin with a slash, indicating that's an absolute path
// - begin without a slash, indicating that's a relative path
//   (relative to the current working directory (AT_FDCWD))
//
// A path may end with a slash, indicating that the caller expects
// the path to be a directory

// The two functions below have circular dependencies, so we need to forward declare them
// Both of them return a referenced dentry, no need to refcount them again
static dentry_t *dentry_resolve_lastseg(dentry_t *parent, char *leaf, lastseg_resolve_flags_t flags, bool *symlink_resolved);
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
static dentry_t *dentry_resolve_to_parent(dentry_t *base_dir, dentry_t *root_dir, const char *original_path, char **last_seg_out)
{
    pr_dinfo2(dcache, "lookup parent of '%s'", original_path);
    MOS_ASSERT_X(base_dir && root_dir && original_path, "Invalid VFS lookup parameters");
    if (last_seg_out != NULL)
        *last_seg_out = NULL;

    dentry_t *parent_ref = statement_expr(dentry_t *, {
        dentry_t *tmp = path_is_absolute(original_path) ? root_dir : base_dir;
        if (tmp->is_mountpoint)
            tmp = dentry_get_mount(tmp)->root; // if it's a mountpoint, jump to mounted filesystem
        tmp = dentry_ref_up_to(tmp, root_dir);
        retval = tmp;
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
        pr_dinfo2(dcache, "lookup parent: current segment '%s'", current_seg);
        const char *const next = strtok_r(NULL, PATH_DELIM_STR, &saveptr);
        if (parent_ref->inode->type == FILE_TYPE_SYMLINK)
        {
            // this is the real interesting dir
            dentry_t *const parent_real_ref = dentry_resolve_follow_symlink(parent_ref, RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_DIR);
            dentry_unref(parent_ref);
            if (IS_ERR(parent_real_ref))
                return ERR_PTR(-ENOENT); // the symlink target does not exist
            parent_ref = parent_real_ref;
        }

        if (next == NULL)
        {
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

            // don't recurse up to the root
            MOS_ASSERT(dentry_unref_one_norelease(parent_ref));
            parent_ref = parent;

            // if the parent is a mountpoint, we need to jump to the mountpoint's parent
            // and then jump to the mountpoint's parent's parent
            // already referenced when we jumped to the mountpoint
            if (parent_ref->is_mountpoint)
                parent_ref = dentry_root_get_mountpoint(parent);

            current_seg = next;
            continue;
        }

        dentry_t *const child_ref = dentry_lookup_child(parent_ref, current_seg);
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
            pr_dinfo2(dcache, "jumping to mountpoint %s", child_ref->name);
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

static dentry_t *dentry_resolve_follow_symlink(dentry_t *d, lastseg_resolve_flags_t flags)
{
    MOS_ASSERT_X(d != NULL && d->inode != NULL, "check before calling this function!");
    MOS_ASSERT_X(d->inode->type == FILE_TYPE_SYMLINK, "check before calling this function!");

    if (!d->inode->ops || !d->inode->ops->readlink)
        mos_panic("inode does not support readlink (symlink) operation, but it's a symlink!");

    char *const target = kmalloc(MOS_PATH_MAX_LENGTH);
    const size_t read = d->inode->ops->readlink(d, target, MOS_PATH_MAX_LENGTH);
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

    pr_dinfo2(dcache, "symlink target: %s", target);

    char *last_segment = NULL;
    dentry_t *const parent_ref = dentry_resolve_to_parent(dentry_parent(d), root_dentry, target, &last_segment);
    kfree(target);
    if (IS_ERR(parent_ref))
        return parent_ref; // the symlink target does not exist

    // it's possibly that the symlink target is also a symlink, this will be handled recursively
    bool is_symlink = false;
    dentry_t *const child_ref = dentry_resolve_lastseg(parent_ref, last_segment, flags, &is_symlink);
    kfree(last_segment);

    // if symlink is true, we need to unref the parent_ref dentry as it's irrelevant now
    if (IS_ERR(child_ref) || is_symlink)
        dentry_unref(parent_ref);

    return child_ref; // the real dentry, or an error code
}

static dentry_t *dentry_resolve_lastseg(dentry_t *parent, char *leaf, lastseg_resolve_flags_t flags, bool *is_symlink)
{
    MOS_ASSERT(parent != NULL && leaf != NULL);
    *is_symlink = false;

    pr_dinfo2(dcache, "resolving last segment: '%s'", leaf);
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
    else if (strncmp(leaf, "..", 3) == 0 || strcmp(leaf, "../") == 0)
    {
        if (parent == root_dentry)
            return parent;

        dentry_t *const parent_parent = dentry_parent(parent);
        MOS_ASSERT(dentry_unref_one_norelease(parent)); // don't recursively unref all the way to the root

        // if the parent is a mountpoint, we need to jump to the mountpoint's parent
        if (parent_parent->is_mountpoint)
            return dentry_root_get_mountpoint(parent_parent);

        return parent_parent;
    }

    dentry_t *const child_ref = dentry_lookup_child(parent, leaf); // now we have a reference to the child

    if (unlikely(child_ref->inode == NULL))
    {
        if (flags & RESOLVE_EXPECT_NONEXIST)
        {
            // do not use dentry_ref, because it checks for an inode
            child_ref->refcount++;
            return child_ref;
        }

        pr_dinfo2(dcache, "file does not exist");
        dentry_try_release(child_ref); // child has no ref, we should release it directly
        return ERR_PTR(-ENOENT);
    }

    MOS_ASSERT(child_ref->refcount > 0); // dentry_get_child may return a negative dentry, which is handled above, otherwise we should have a reference on it

    if (flags & RESOLVE_EXPECT_NONEXIST && !(flags & RESOLVE_EXPECT_EXIST))
    {
        dentry_unref(child_ref);
        return ERR_PTR(-EEXIST);
    }

    if (child_ref->inode->type == FILE_TYPE_SYMLINK)
    {
        if (!(flags & RESOLVE_SYMLINK_NOFOLLOW))
        {
            pr_dinfo2(dcache, "resolving symlink for '%s'", leaf);
            dentry_t *const symlink_target_ref = dentry_resolve_follow_symlink(child_ref, flags);
            // we don't need the symlink node anymore
            MOS_ASSERT(dentry_unref_one_norelease(child_ref));
            *is_symlink = symlink_target_ref != NULL;
            return symlink_target_ref;
        }

        pr_dinfo2(dcache, "not following symlink");
    }
    else if (child_ref->inode->type == FILE_TYPE_DIRECTORY)
    {
        if (!(flags & RESOLVE_EXPECT_DIR))
        {
            MOS_ASSERT(dentry_unref_one_norelease(child_ref)); // it's the caller's responsibility to unref the parent and grandparents
            return ERR_PTR(-EISDIR);
        }

        // if the child is a mountpoint, we need to jump to the mounted filesystem's root
        if (child_ref->is_mountpoint)
            return dentry_ref(dentry_get_mount(child_ref)->root);
    }
    else
    {
        if (!(flags & RESOLVE_EXPECT_FILE))
        {
            MOS_ASSERT(dentry_unref_one_norelease(child_ref)); // it's the caller's responsibility to unref the parent and grandparents
            return ERR_PTR(-ENOTDIR);
        }
    }

    return child_ref;
}

void dentry_attach(dentry_t *d, inode_t *inode)
{
    MOS_ASSERT_X(d->inode == NULL, "reattaching an inode to a dentry");
    MOS_ASSERT(inode != NULL);
    // MOS_ASSERT_X(d->refcount == 1, "dentry %p refcount %zu is not 1", (void *) d, d->refcount);

    for (atomic_t i = 0; i < d->refcount; i++)
        inode_ref(inode); // refcount the inode for each reference to the dentry

    inode_ref(inode); // refcount the inode for each reference to the dentry
    d->inode = inode;
}

void dentry_detach(dentry_t *d)
{
    if (d->inode == NULL)
        return;

    // the caller should have the only reference to the dentry
    // MOS_ASSERT(d->refcount == 1); // !! TODO: this assertion fails in vfs_unlinkat

    (void) inode_unref(d->inode); // we don't care if the inode is freed or not
    d->inode = NULL;
}

dentry_t *dentry_from_fd(fd_t fd)
{
    if (fd == AT_FDCWD)
    {
        if (current_thread)
            return current_process->working_directory;
        else
            return root_dentry; // no current process, so cwd is always root
    }

    // sanity check: fd != AT_FDCWD, no current process
    MOS_ASSERT(current_thread);

    io_t *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return ERR_PTR(-EBADF);

    if (io->type != IO_FILE && io->type != IO_DIR)
        return ERR_PTR(-EBADF);

    file_t *file = container_of(io, file_t, io);
    return file->dentry;
}

dentry_t *dentry_lookup_child(dentry_t *parent, const char *name)
{
    if (unlikely(parent == NULL))
        return NULL;

    pr_dinfo2(dcache, "looking for dentry '%s' in '%s'", name, dentry_name(parent));

    // firstly check if it's in the cache
    dentry_t *dentry = dentry_get_from_parent(parent->superblock, parent, name);
    MOS_ASSERT(dentry);

    spinlock_acquire(&dentry->lock);

    if (dentry->inode)
    {
        pr_dinfo2(dcache, "dentry '%s' found in the cache", name);
        spinlock_release(&dentry->lock);
        return dentry_ref(dentry);
    }

    // not in the cache, try to find it in the filesystem
    if (parent->inode == NULL || parent->inode->ops == NULL || parent->inode->ops->lookup == NULL)
    {
        pr_dinfo2(dcache, "filesystem doesn't support lookup");
        spinlock_release(&dentry->lock);
        return dentry;
    }

    const bool lookup_result = parent->inode->ops->lookup(parent->inode, dentry);
    spinlock_release(&dentry->lock);

    if (lookup_result)
    {
        pr_dinfo2(dcache, "dentry '%s' found in the filesystem", name);
        return dentry_ref(dentry);
    }
    else
    {
        pr_dinfo2(dcache, "dentry '%s' not found in the filesystem", name);
        return dentry; // do not reference a negative dentry
    }
}

dentry_t *dentry_resolve(dentry_t *starting_dir, dentry_t *root_dir, const char *path, lastseg_resolve_flags_t flags)
{
    if (!root_dir)
        return ERR_PTR(-ENOENT); // no root directory

    char *last_segment;
    pr_dinfo2(dcache, "resolving path '%s'", path);
    dentry_t *const parent_ref = dentry_resolve_to_parent(starting_dir, root_dir, path, &last_segment);
    if (IS_ERR(parent_ref))
    {
        pr_dinfo2(dcache, "failed to resolve parent of '%s', file not found", path);
        return parent_ref;
    }

    if (last_segment == NULL)
    {
        // path is a single "/"
        pr_dinfo2(dcache, "path '%s' is a single '/' or is empty", path);
        MOS_ASSERT(parent_ref == starting_dir);
        if (!(flags & RESOLVE_EXPECT_DIR))
        {
            dentry_unref(parent_ref);
            return ERR_PTR(-EISDIR);
        }

        return parent_ref;
    }

    bool symlink = false;
    dentry_t *child_ref = dentry_resolve_lastseg(parent_ref, last_segment, flags, &symlink);
    kfree(last_segment);
    if (IS_ERR(child_ref) || symlink)
        dentry_unref(parent_ref); // the lookup failed, or child_ref is irrelevant with the parent_ref

    return child_ref;
}

static void dirter_add(vfs_listdir_state_t *state, u64 ino, const char *name, size_t name_len, file_type_t type)
{
    vfs_listdir_entry_t *entry = kmalloc(sizeof(vfs_listdir_entry_t));
    linked_list_init(list_node(entry));
    entry->ino = ino;
    entry->name = strndup(name, name_len);
    entry->name_len = name_len;
    entry->type = type;
    list_node_append(&state->entries, list_node(entry));
    state->n_count++;
}

void vfs_populate_listdir_buf(dentry_t *dir, vfs_listdir_state_t *state)
{
    // this call may not write all the entries, because the buffer may not be big enough
    if (dir->inode->ops && dir->inode->ops->iterate_dir)
        dir->inode->ops->iterate_dir(dir, state, dirter_add);
    else
        vfs_generic_iterate_dir(dir, state, dirter_add);
}
