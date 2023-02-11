// SPDX-License-Identifier: GPL-3.0-or-later
// Virtual File System Public API

#pragma once

#include "lib/sync/mutex.h"
#include "mos/filesystem/fs_types.h"
#include "mos/tasks/task_types.h"
#include "mos/types.h"

typedef enum
{
    FILE_OPEN_READ = IO_READABLE,  // 1 << 0
    FILE_OPEN_WRITE = IO_WRITABLE, // 1 << 1
    FILE_OPEN_NO_FOLLOW = 1 << 2,
    FILE_OPEN_CREATE = 1 << 3,
    FILE_OPEN_EXECUTE = 1 << 4,
} file_open_flags;

always_inline void file_format_perm(file_perm_t perms, char buf[10])
{
    buf[0] = perms.owner.read ? 'r' : '-';
    buf[1] = perms.owner.write ? 'w' : '-';
    buf[2] = perms.owner.execute ? 'x' : '-';
    buf[3] = perms.group.read ? 'r' : '-';
    buf[4] = perms.group.write ? 'w' : '-';
    buf[5] = perms.group.execute ? 'x' : '-';
    buf[6] = perms.others.read ? 'r' : '-';
    buf[7] = perms.others.write ? 'w' : '-';
    buf[8] = perms.others.execute ? 'x' : '-';
    buf[9] = '\0';
}

extern dentry_t *root_dentry;

void vfs_init(void);

void vfs_register_filesystem(filesystem_t *fs);

/**
 * @brief Mount a filesystem at a given existing path
 *
 * @param device The device to mount
 * @param path The path to mount the filesystem at, this absolute path to a directory must exist
 * @param fs The filesystem type, e.g. "tmpfs"
 * @param options The options to pass to the filesystem
 * @return true if the filesystem was mounted successfully
 * @return false if the filesystem could not be mounted, see the kernel log for more information
 */
bool vfs_mount(const char *device, const char *path, const char *fs, const char *options);

file_t *vfs_open(const char *path, file_open_flags flags);
file_t *vfs_openat(int fd, const char *path, file_open_flags flags);

/**
 * @brief Stat a file
 *
 * @param path
 * @param stat
 * @return true
 * @return false
 */
bool vfs_stat(const char *path, file_stat_t *restrict stat);

/**
 * @brief Read a symbolic link
 *
 * @param path The path to the symbolic link
 * @param buf The buffer to store the link in
 * @param size The size of the buffer
 * @return size_t The size of the link, or 0 if the link could not be read, or the buffer was too small
 */
size_t vfs_readlink(const char *path, char *buf, size_t size);

/**
 * @brief Create a new file
 *
 * @param path The path to the file
 * @param perms The permissions of the file, if the file already exists, the permissions will not be changed
 * @return true if the file was created successfully, or already exists. false if the file could not be created
 */
bool vfs_touch(const char *path, file_type_t type, u32 perms);

/**
 * @brief Create a symbolic link
 *
 * @param path The path to the symbolic link
 * @param target The target of the symbolic link
 * @return true
 * @return false
 */
bool vfs_symlink(const char *path, const char *target);

/**
 * @brief Create a directory
 *
 * @param path The path to the directory
 * @return true
 * @return false
 */
bool vfs_mkdir(const char *path);
