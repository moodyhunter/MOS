// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs_types.h>
#include <mos/lib/structures/list.h>
#include <mos/mos_global.h>

/**
 * @defgroup dentry Directory Entry
 * @brief Directory entry
 * @ingroup vfs
 *
 * @details
 * A dentry is a directory entry, it is a reference to an inode.
 *
 * dentry cache policy: The function who references the dentry should be
 * responsible for unrefing it.
 *
 * All existing files' dentries have a reference count of 0 at the start.
 * When a file is opened, the dentry will be referenced, and the reference
 * count will be incremented by 1.
 *
 * For all directories, the initial reference count is also 0, but when
 * a directory is opened, the reference count will be incremented by 1.
 *
 * When mounting a filesystem, the root dentry of the filesystem is
 * inserted into the dentry cache, and will have a reference count of 1.
 * The mountpoint itself will have its reference count incremented by 1.
 *
 * For the root dentry ("/"), the reference count is 2, one for the mountpoint,
 * and one for the dentry cache.
 * @{
 */

typedef enum
{
    // bit 0, 1: the operation only succeeds if the inode is a...
    RESOLVE_EXPECT_FILE = 1 << 0,
    RESOLVE_EXPECT_DIR = 1 << 1,
    RESOLVE_EXPECT_ANY_TYPE = RESOLVE_EXPECT_FILE | RESOLVE_EXPECT_DIR,

    // bit 2: follow symlinks?
    // only for the last segment, (if it is a symlink)
    RESOLVE_SYMLINK_NOFOLLOW = 1 << 2,

    // bit 3, 4: the operation only succeeds if...
    RESOLVE_EXPECT_EXIST = 1 << 3,
    RESOLVE_EXPECT_NONEXIST = 1 << 4,
    RESOLVE_EXPECT_ANY_EXIST = RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_NONEXIST,
} lastseg_resolve_flags_t;

/**
 * @brief Check if a path is absolute
 *
 * @param path The path to check
 * @return true if the path is absolute (starts with a '/'), false otherwise
 */
should_inline bool path_is_absolute(const char *path)
{
    return path[0] == '/';
}

should_inline dentry_t *dentry_parent(const dentry_t *dentry)
{
    return tree_parent(dentry, dentry_t);
}

/**
 * @brief A linked-list of all mountpoints
 *
 */
extern list_head vfs_mountpoint_list;

/**
 * @brief Increment the reference count of a dentry
 *
 * @param dentry The dentry to increment the reference count of
 * @return the dentry itself
 */
dentry_t *dentry_ref(dentry_t *dentry);

/**
 * @brief Increment the reference count of a dentry up to a given dentry
 *
 * @param dentry The dentry to increment the reference count of
 * @param root The dentry to stop at
 * @return dentry_t* The dentry itself
 */
dentry_t *dentry_ref_up_to(dentry_t *dentry, dentry_t *root);

/**
 * @brief Decrement the reference count of a dentry
 *
 * @param dentry The dentry to decrement the reference count of
 */
void dentry_unref(dentry_t *dentry);

/**
 * @brief Get the dentry from a file descriptor
 *
 * @param fd The file descriptor, there's a special case for FD_CWD, which corresponds to the process's current working directory
 *
 * @return The dentry associated with the file descriptor, or NULL if the file descriptor is invalid
 */
dentry_t *dentry_from_fd(fd_t fd);

/**
 * @brief Get a child dentry from a parent dentry
 *
 * @param parent The parent dentry
 * @param name The name of the child dentry
 *
 * @return The child dentry, always non-NULL, even if the child dentry does not exist in the filesystem
 * @note The returned dentry will have its reference count incremented, even if it does not exist.
 */
dentry_t *dentry_get_child(dentry_t *parent, const char *name);

/**
 * @brief Lookup a path in the filesystem
 *
 * @details If the path is absolute, the base is ignored and the path starts from the root_dir
 *          If the path is relative, the base is used as the starting points
 *
 * @param starting_dir The starting directory when resolving a [relative] path
 * @param root_dir the root directory when resolving the path, the resolved path will not go above this directory
 * @param path The path to resolve, can be absolute or relative
 * @param flags Flags to control the behavior of the path resolution, see \ref lastseg_resolve_flags_t
 *
 * @return The parent directory (if such a parent exists) containing (or to contain) the last segment of the path, or
 *         NULL if any intermediate directory in the path does not exist.
 *
 */
dentry_t *dentry_get(dentry_t *starting_dir, dentry_t *root_dir, const char *path, lastseg_resolve_flags_t flags);

/**
 * @brief Mount a filesystem at a mountpoint
 *
 * @param mountpoint The mountpoint
 * @param root The root directory of the filesystem
 * @param fs The filesystem to mount
 *
 * @return true if the filesystem was mounted successfully, false otherwise
 */
__nodiscard bool dentry_mount(dentry_t *mountpoint, dentry_t *root, filesystem_t *fs);

/**
 * @brief List the contents of a directory
 *
 * @param dir The directory to list
 * @param state The state of the directory iterator
 *
 * @return The number of bytes written to the buffer, which is contained in the state object
 */
size_t dentry_list(dentry_t *dir, dir_iterator_state_t *state);

/**
 * @brief Get the path of a dentry
 *
 * @param dentry The dentry to get the path of
 * @param root The root directory, the path will not go above this directory
 * @param buf The buffer to write the path to
 * @param size The size of the buffer
 * @return ssize_t
 */
ssize_t dentry_path(dentry_t *dentry, dentry_t *root, char *buf, size_t size);

/**@}*/
