// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fs_types.h"

typedef enum
{
    LASTSEG_DIRECTORY = 1 << 0,
    LASTSEG_FILE = 1 << 1,
    LASTSEG_FOLLOW_SYMLINK = 1 << 2,
    LASTSEG_ALLOW_NONEXISTENT = 1 << 3,
} lastseg_resolve_flags_t;

/**
 * @brief Initialize the dentry cache and mountpoint map
 */
void dentry_init(void);

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
 * @brief Create a new dentry with the given name and parent
 *
 * @arg name The name of the dentry
 * @arg parent The parent dentry
 *
 * @return The new dentry, or NULL if the dentry could not be created
 * @note The returned dentry will have its reference count incremented.
 */
dentry_t *dentry_create(dentry_t *parent, const char *name);

/**
 * @brief Get the dentry from a file descriptor
 *
 * @arg fd The file descriptor, there's a special case for FD_CWD, which corresponds to the process's current working
 *         directory
 *
 * @return The dentry associated with the file descriptor, or NULL if the file descriptor is invalid
 */
dentry_t *dentry_from_fd(fd_t fd);

/**
 * @brief  Get a child dentry from a parent dentry
 *
 * @arg parent The parent dentry
 * @arg name The name of the child dentry
 *
 * @return The child dentry, or NULL if the child dentry does not exist
 * @note The returned dentry will have its reference count incremented.
 */
dentry_t *dentry_get_child(const dentry_t *parent, const char *name);

/**
 * @brief Lookup a path in the filesystem
 *
 * @details If the path is absolute, the base is ignored and the path starts from the root_dir
 *          If the path is relative, the base is used as the starting points
 *
 * @arg base The base directory when resolving the path, this is used as a starting point when the path is relative
 * @arg root_dirthe root directory when resolving the path, the resolved path will not go above this directory
 * @arg path The path to resolve, can be absolute or relative
 * @arg last_seg_out The last segment of the path, it's the caller's responsibility to free this memory, can be NULL if
 *                   the caller doesn't need it
 *
 * @return The parent directory (if such a parent exists) containing (or to contain) the last segment of the path, or
 *         NULL if any intermediate directory in the path does not exist.
 *
 */
dentry_t *dentry_lookup(dentry_t *base, dentry_t *root_dir, const char *path, lastseg_resolve_flags_t flags);

__nodiscard bool dentry_mount(dentry_t *mountpoint, dentry_t *root);
