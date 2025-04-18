// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/dentry.hpp"

#include "mos/assert.hpp"
#include "mos/filesystem/inode.hpp"
#include "mos/filesystem/mount.hpp"
#include "mos/filesystem/vfs.hpp"
#include "mos/filesystem/vfs_types.hpp"
#include "mos/filesystem/vfs_utils.hpp"
#include "mos/io/io.hpp"
#include "mos/lib/sync/spinlock.hpp"
#include "mos/misc/kutils.hpp"
#include "mos/tasks/process.hpp"
#include "mos/tasks/task_types.hpp"

#include <atomic>
#include <mos/filesystem/fs_types.h>
#include <mos/lib/structures/hashmap_common.hpp>
#include <mos_stdio.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

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
static PtrResult<dentry_t> dentry_resolve_lastseg(dentry_t *parent, mos::string leaf, const LastSegmentResolveFlags flags, bool *is_symlink);
static PtrResult<dentry_t> dentry_resolve_follow_symlink(dentry_t *dentry, LastSegmentResolveFlags flags);

/**
 * @brief Lookup the parent directory of a given path, and return the last segment of the path in last_seg_out
 *
 * @param base_dir A directory to start the lookup from
 * @param root_dir The root directory of the filesystem, the lookup will not go above this directory
 * @param original_path The path to lookup
 * @param last_seg_out The last segment of the path will be returned in this parameter, the caller is responsible for freeing it
 * @return dentry_t* The parent directory of the path, or NULL if the path is invalid, the dentry will be referenced
 */
static std::pair<PtrResult<dentry_t>, std::optional<mos::string>> dentry_resolve_to_parent(dentry_t *base_dir, dentry_t *root_dir, mos::string_view path)
{
    dInfo2<dcache> << "lookup parent of '" << path << "'";
    MOS_ASSERT_X(base_dir && root_dir, "Invalid VFS lookup parameters");

    dentry_t *parent_ref = [&]()
    {
        dentry_t *tmp = path_is_absolute(path) ? root_dir : base_dir;
        if (tmp->is_mountpoint)
            tmp = dentry_get_mount(tmp)->root; // if it's a mountpoint, jump to mounted filesystem
        return dentry_ref_up_to(tmp, root_dir);
    }();

    const auto parts = split_string(path, PATH_DELIM);
    if (unlikely(parts.empty()))
    {
        // this only happens if the path is empty, or contains only slashes
        // in which case we return the base directory
        return { parent_ref, std::nullopt };
    }

    for (size_t i = 0; i < parts.size(); i++)
    {
        const bool is_last = i == parts.size() - 1;
        const auto current_seg = parts[i];

        dInfo2<dcache> << "lookup parent: current segment '" << current_seg << "'" << (is_last ? " (last)" : "");

        if (is_last)
        {
            const bool ends_with_slash = path.ends_with(PATH_DELIM);
            return { parent_ref, current_seg + (ends_with_slash ? PATH_DELIM_STR : "") };
        }

        if (current_seg == "." || current_seg == "./")
            continue;

        if (current_seg == ".." || current_seg == "../")
        {
            // we can't go above the root directory
            if (parent_ref != root_dir)
            {
                dentry_t *const parent = dentry_parent(*parent_ref);

                // don't recurse up to the root
                MOS_ASSERT(dentry_unref_one_norelease(parent_ref));
                parent_ref = parent;

                // if the parent is a mountpoint, we need to jump to the mountpoint's parent
                // and then jump to the mountpoint's parent's parent
                // already referenced when we jumped to the mountpoint
                if (parent_ref->is_mountpoint)
                    parent_ref = dentry_root_get_mountpoint(parent);
            }
        }
        else
        {
            auto child_ref = dentry_lookup_child(parent_ref, current_seg);
            if (child_ref->inode == NULL)
            {
                // kfree(path);
                dentry_try_release(child_ref.get());
                dentry_unref(parent_ref);
                return { -ENOENT, std::nullopt };
            }

            if (child_ref->is_mountpoint)
            {
                dInfo2<dcache> << "jumping to mountpoint " << child_ref->name;
                parent_ref = dentry_get_mount(child_ref.get())->root; // if it's a mountpoint, jump to the tree of mounted filesystem instead

                // refcount the mounted filesystem root
                dentry_ref(parent_ref);
            }
            else
            {
                parent_ref = child_ref.get();
            }
        }

        if (parent_ref->inode->type == FILE_TYPE_SYMLINK)
        {
            // go to the real interesting dir (if it's a symlink)
            auto parent_real_ref = dentry_resolve_follow_symlink(parent_ref, RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_DIR);
            dentry_unref(parent_ref);
            if (parent_real_ref.isErr())
                return { -ENOENT, std::nullopt }; // the symlink target does not exist
            parent_ref = parent_real_ref.get();
        }
    }

    MOS_UNREACHABLE();
}

static PtrResult<dentry_t> dentry_resolve_follow_symlink(dentry_t *d, LastSegmentResolveFlags flags)
{
    MOS_ASSERT_X(d != NULL && d->inode != NULL, "check before calling this function!");
    MOS_ASSERT_X(d->inode->type == FILE_TYPE_SYMLINK, "check before calling this function!");

    if (!d->inode->ops || !d->inode->ops->readlink)
        mos_panic("inode does not support readlink (symlink) operation, but it's a symlink!");

    const auto target = (char *) kcalloc<char>(MOS_PATH_MAX_LENGTH);
    const size_t read = d->inode->ops->readlink(d, target, MOS_PATH_MAX_LENGTH);
    if (read == 0)
    {
        mos_warn("symlink is empty");
        return -ENOENT; // symlink is empty
    }

    if (read == MOS_PATH_MAX_LENGTH)
    {
        mos_warn("symlink is too long");
        return -ENAMETOOLONG; // symlink is too long
    }

    target[read] = '\0'; // ensure null termination

    dInfo2<dcache> << "symlink target: " << target;

    auto [parent_ref, last_segment] = dentry_resolve_to_parent(dentry_parent(*d), root_dentry, target);
    kfree(target);
    if (parent_ref.isErr())
        return parent_ref; // the symlink target does not exist

    // it's possibly that the symlink target is also a symlink, this will be handled recursively
    bool is_symlink = false;
    const auto child_ref = dentry_resolve_lastseg(parent_ref.get(), *last_segment, flags, &is_symlink);

    // if symlink is true, we need to unref the parent_ref dentry as it's irrelevant now
    if (child_ref.isErr() || is_symlink)
        dentry_unref(parent_ref.get());

    return child_ref; // the real dentry, or an error code
}

static PtrResult<dentry_t> dentry_resolve_lastseg(dentry_t *parent, mos::string leaf, const LastSegmentResolveFlags flags, bool *is_symlink)
{
    MOS_ASSERT(parent != NULL);
    *is_symlink = false;

    dInfo2<dcache> << "resolving last segment: '" << leaf << "'";
    const bool ends_with_slash = leaf.ends_with(PATH_DELIM);
    if (ends_with_slash)
        leaf.resize(leaf.size() - 1); // remove the trailing slash

    if (unlikely(ends_with_slash && !flags.test(RESOLVE_EXPECT_DIR)))
    {
        mos_warn("RESOLVE_EXPECT_DIR isn't set, but the provided path ends with a slash");
        return -EINVAL;
    }

    if (leaf == "." || leaf == "./")
        return parent;
    else if (leaf == ".." || leaf == "../")
    {
        if (parent == root_dentry)
            return parent;

        dentry_t *const parent_parent = dentry_parent(*parent);
        MOS_ASSERT(dentry_unref_one_norelease(parent)); // don't recursively unref all the way to the root

        // if the parent is a mountpoint, we need to jump to the mountpoint's parent
        if (parent_parent->is_mountpoint)
            return dentry_root_get_mountpoint(parent_parent);

        return parent_parent;
    }

    auto child_ref = dentry_lookup_child(parent, leaf); // now we have a reference to the child

    if (unlikely(child_ref->inode == NULL))
    {
        if (flags.test(RESOLVE_EXPECT_NONEXIST))
        {
            // do not use dentry_ref, because it checks for an inode
            child_ref->refcount++;
            return child_ref;
        }

        dInfo2<dcache> << "file does not exist";
        dentry_try_release(child_ref.get()); // child has no ref, we should release it directly
        return -ENOENT;
    }

    MOS_ASSERT(child_ref->refcount > 0); // dentry_get_child may return a negative dentry, which is handled above, otherwise we should have a reference on it

    if (flags.test(RESOLVE_EXPECT_NONEXIST) && !flags.test(RESOLVE_EXPECT_EXIST))
    {
        dentry_unref(child_ref.get());
        return -EEXIST;
    }

    if (child_ref->inode->type == FILE_TYPE_SYMLINK)
    {
        if (!flags.test(RESOLVE_SYMLINK_NOFOLLOW))
        {
            dInfo2<dcache> << "resolving symlink for '" << leaf << "'";
            const auto symlink_target_ref = dentry_resolve_follow_symlink(child_ref.get(), flags);
            // we don't need the symlink node anymore
            MOS_ASSERT(dentry_unref_one_norelease(child_ref.get()));
            *is_symlink = symlink_target_ref != nullptr;
            return symlink_target_ref;
        }

        dInfo2<dcache> << "not following symlink";
    }
    else if (child_ref->inode->type == FILE_TYPE_DIRECTORY)
    {
        if (!flags.test(RESOLVE_EXPECT_DIR))
        {
            MOS_ASSERT(dentry_unref_one_norelease(child_ref.get())); // it's the caller's responsibility to unref the parent and grandparents
            return -EISDIR;
        }

        // if the child is a mountpoint, we need to jump to the mounted filesystem's root
        if (child_ref->is_mountpoint)
            return dentry_ref(dentry_get_mount(child_ref.get())->root);
    }
    else
    {
        if (!flags.test(RESOLVE_EXPECT_FILE))
        {
            MOS_ASSERT(dentry_unref_one_norelease(child_ref.get())); // it's the caller's responsibility to unref the parent and grandparents
            return -ENOTDIR;
        }
    }

    return child_ref;
}

void dentry_attach(dentry_t *d, inode_t *inode)
{
    MOS_ASSERT_X(d->inode == NULL, "reattaching an inode to a dentry");
    MOS_ASSERT(inode != NULL);
    // MOS_ASSERT_X(d->refcount == 1, "dentry %p refcount %zu is not 1", (void *) d, d->refcount);

    for (std::atomic_size_t i = 0; i < d->refcount; i++)
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

PtrResult<dentry_t> dentry_from_fd(fd_t fd)
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

    IO *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return -EBADF;

    if (io->io_type != IO_FILE && io->io_type != IO_DIR)
        return -EBADF;

    FsBaseFile *file = static_cast<FsBaseFile *>(io);
    return file->dentry;
}

PtrResult<dentry_t> dentry_lookup_child(dentry_t *parent, mos::string_view name)
{
    if (unlikely(parent == nullptr))
        return nullptr;

    dInfo2<dcache> << "looking for dentry '" << name.data() << "' in '" << dentry_name(parent) << "'";

    // firstly check if it's in the cache
    dentry_t *dentry = dentry_get_from_parent(parent->superblock, parent, name);
    MOS_ASSERT(dentry);

    spinlock_acquire(&dentry->lock);

    if (dentry->inode)
    {
        dInfo2<dcache> << "dentry '" << name.data() << "' found in the cache";
        spinlock_release(&dentry->lock);
        return dentry_ref(dentry);
    }

    // not in the cache, try to find it in the filesystem
    if (parent->inode == NULL || parent->inode->ops == NULL || parent->inode->ops->lookup == NULL)
    {
        dInfo2<dcache> << "filesystem doesn't support lookup";
        spinlock_release(&dentry->lock);
        return dentry;
    }

    const bool lookup_result = parent->inode->ops->lookup(parent->inode, dentry);
    spinlock_release(&dentry->lock);

    if (lookup_result)
    {
        dInfo2<dcache> << "dentry '" << name.data() << "' found in the filesystem";
        return dentry_ref(dentry);
    }
    else
    {
        dInfo2<dcache> << "dentry '" << name.data() << "' not found in the filesystem";
        return dentry; // do not reference a negative dentry
    }
}

PtrResult<dentry_t> dentry_resolve(dentry_t *starting_dir, dentry_t *root_dir, mos::string_view path, LastSegmentResolveFlags flags)
{
    if (!root_dir)
        return -ENOENT; // no root directory

    dInfo2<dcache> << "resolving path '" << path << "'";
    const auto [parent_ref, last_segment] = dentry_resolve_to_parent(starting_dir, root_dir, path);
    if (parent_ref.isErr())
    {
        dInfo2<dcache> << "failed to resolve parent of '" << path << "', file not found";
        return parent_ref;
    }

    if (!last_segment)
    {
        // path is a single "/", last_segment is empty
        dInfo2<dcache> << "path '" << path << "' is a single '/' or is empty";
        MOS_ASSERT(parent_ref == starting_dir);
        if (!flags.test(RESOLVE_EXPECT_DIR))
        {
            dentry_unref(parent_ref.get());
            return -EISDIR;
        }

        return parent_ref;
    }

    bool symlink = false;
    auto child_ref = dentry_resolve_lastseg(parent_ref.get(), *last_segment, flags, &symlink);
    if (child_ref.isErr() || symlink)
        dentry_unref(parent_ref.get()); // the lookup failed, or child_ref is irrelevant with the parent_ref
    return child_ref;
}

static void dirter_add(vfs_listdir_state_t *state, u64 ino, mos::string_view name, file_type_t type)
{
    vfs_listdir_entry_t *entry = mos::create<vfs_listdir_entry_t>();
    linked_list_init(list_node(entry));
    entry->ino = ino;
    entry->name = name;
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
