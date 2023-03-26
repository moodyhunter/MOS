// SPDX-License-Identifier: GPL-3.0-or-later
// Virtual File System Public API

#pragma once

#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs_types.h>
#include <mos/lib/sync/mutex.h>
#include <mos/types.h>

typedef struct _process process_t; // forward declaration

should_inline const file_ops_t *file_get_ops(file_t *file)
{
    return file->dentry->inode->file_ops;
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

file_t *vfs_open(const char *path, open_flags flags);
file_t *vfs_openat(int fd, const char *path, open_flags flags);

/**
 * @brief Stat a file
 *
 * @param path The path to the file
 * @param stat The stat struct to store the file information in
 * @return true
 * @return false
 */
bool vfs_stat(const char *path, file_stat_t *restrict stat);

/**
 * @brief Stat an opened file
 *
 * @param io_t The io object of a file_t
 * @param stat The stat struct to store the file information in
 * @return true
 * @return false
 */
bool vfs_fstat(io_t *io, file_stat_t *restrict stat);

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

/**
 * @brief Get the content of a directory
 *
 * @param io The io object of a file_t, must be a directory and opened with FILE_OPEN_READ
 * @param buf The buffer to store the directory entries in
 * @param size The size of the buffer
 *
 * @return size_t The number of bytes read, or 0 if the end of the directory was reached
 */
size_t vfs_list_dir(io_t *io, char *buf, size_t size);

/**
 * @brief Change the current working directory
 *
 * @param path The path to the new working directory
 * @return true
 * @return false
 */
bool vfs_chdir(const char *path);

/**
 * @brief Get the current working directory
 *
 * @param buf The buffer to store the path in
 * @param size The size of the buffer
 * @return ssize_t The size of the path, or 0 if the buffer was too small
 */
ssize_t vfs_getcwd(char *buf, size_t size);
