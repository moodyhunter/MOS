// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/fs_types.h"
#include "mos/filesystem/vfs_types.h"

typedef enum
{
    // bit 0, 1: the operation only succeeds if the inode is a...
    RESOLVE_EXPECT_FILE = 1 << 0,
    RESOLVE_EXPECT_DIR = 1 << 1,
    RESOLVE_ANY = RESOLVE_EXPECT_FILE | RESOLVE_EXPECT_DIR,

    // bit 2: follow symlinks?
    // only for the last segment, (if it is a symlink)
    RESOLVE_SYMLINK_NOFOLLOW = 1 << 2,

    // bit 3, 4: the operation only succeeds if...
    RESOLVE_EXPECT_EXIST = 1 << 3,
    RESOLVE_EXPECT_NONEXIST = 1 << 4,

    // bit 5: the operation will...
    RESOLVE_WILL_CREATE = 1 << 5, // create the file if it doesn't exist

    // compose the flags
    RESOLVE_CREATE_ONLY = RESOLVE_EXPECT_NONEXIST | RESOLVE_WILL_CREATE,
    RESOLVE_CREATE_IF_NONEXIST = RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_NONEXIST | RESOLVE_WILL_CREATE,
    RESOLVE_FOR_STAT = RESOLVE_EXPECT_FILE | RESOLVE_EXPECT_DIR | RESOLVE_EXPECT_EXIST,
} lastseg_resolve_flags_t;

/**
 * @brief Initialize the dentry cache and mountpoint map
 */
void dentry_init(void);

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

/**
 * @brief Increment the reference count of a dentry
 *
 * @param dentry The dentry to increment the reference count of
 * @return the dentry itself
 */
should_inline dentry_t *dentry_ref(dentry_t *dentry)
{
    dentry->refcount++;
    return dentry;
}

/**
 * @brief Decrement the reference count of a dentry
 *
 * @param dentry The dentry to decrement the reference count of
 */
void dentry_unref(dentry_t *dentry);

/**
 * @brief Create a new dentry with the given name and parent
 *
 * @param name The name of the dentry
 * @param parent The parent dentry
 *
 * @return The new dentry, or NULL if the dentry could not be created
 * @note The returned dentry will have its reference count incremented.
 */
dentry_t *dentry_create(dentry_t *parent, const char *name);

/**
 * @brief Get the dentry from a file descriptor
 *
 * @param fd The file descriptor, there's a special case for FD_CWD, which corresponds to the process's current working directory
 *
 * @return The dentry associated with the file descriptor, or NULL if the file descriptor is invalid
 */
dentry_t *dentry_from_fd(fd_t fd);

/**
 * @brief  Get a child dentry from a parent dentry
 *
 * @param parent The parent dentry
 * @param name The name of the child dentry
 *
 * @return The child dentry, or NULL if the child dentry does not exist
 * @note The returned dentry will have its reference count incremented.
 */
dentry_t *dentry_get_child(dentry_t *parent, const char *name);

/**
 * @brief Lookup a path in the filesystem
 *
 * @details If the path is absolute, the base is ignored and the path starts from the root_dir
 *          If the path is relative, the base is used as the starting points
 *
 * @param base The base directory when resolving the path, this is used as a starting point when the path is relative
 * @param root_dir the root directory when resolving the path, the resolved path will not go above this directory
 * @param path The path to resolve, can be absolute or relative
 * @param flags Flags to control the behavior of the path resolution, see \ref lastseg_resolve_flags_t
 *
 * @return The parent directory (if such a parent exists) containing (or to contain) the last segment of the path, or
 *         NULL if any intermediate directory in the path does not exist.
 *
 */
dentry_t *dentry_get(dentry_t *base, dentry_t *root_dir, const char *path, lastseg_resolve_flags_t flags);

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
