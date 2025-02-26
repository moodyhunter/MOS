// SPDX-License-Identifier: GPL-3.0-or-later
// Virtual File System Public API

#pragma once

#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs_types.hpp>
#include <mos/lib/sync/mutex.hpp>
#include <mos/types.hpp>

/**
 * @defgroup vfs Virtual File System
 * @brief The Virtual File System (VFS) is an abstraction layer that allows the kernel to
 * interact with different filesystems in a uniform way.
 * @{
 */

extern dentry_t *root_dentry;

/**
 * @brief Open an directory dentry
 *
 * @param entry
 * @param created
 * @param read
 * @param write
 * @param exec
 * @param truncate
 * @return file_t
 */
PtrResult<BasicFile> vfs_do_open_dentry(dentry_t *entry, bool created, bool read, bool write, bool exec, bool truncate);

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
PtrResult<void> vfs_mount(const char *device, const char *path, const char *fs, const char *options);

/**
 * @brief Unmount a filesystem at a given path
 *
 * @param path The path to the filesystem
 * @return true
 * @return false
 */
long vfs_unmount(const char *path);

/**
 * @brief Open a file at a given path
 *
 * @param fd The fd of an open directory, or AT_FDCWD to use the current working directory
 * @param path The path to the file, can be absolute or relative
 * @param flags open_flags flags
 * @return file_t* The file, or NULL if the file could not be opened
 */
PtrResult<BasicFile> vfs_openat(int fd, const char *path, OpenFlags flags);

/**
 * @brief Stat a file
 *
 * @param fd The directory, or a file if FSTATAT_FILE is set
 * @param path The path to the file
 * @param flags fstat_flags flags
 * @param stat The stat struct to store the file information in
 *
 * @details
 * If the FSTATAT_FILE is set, the fd is a file, and the path will be ignored.
 * If it is not set:
 *     If the path is absolute, the fd will be ignored.
 *     If the path is relative, the fd will be used as the directory to resolve the path from.
 * If FSTATAT_NOFOLLOW is set, when the path is used, symlinks will not be followed.
 */
long vfs_fstatat(fd_t fd, const char *path, file_stat_t *__restrict stat, FStatAtFlags flags);

/**
 * @brief Read a symbolic link
 *
 * @param dirfd The file descriptor of the directory containing the symbolic link
 * @param path The path to the symbolic link
 * @param buf The buffer to store the link in
 * @param size The size of the buffer
 * @return size_t The size of the link, or 0 if the link could not be read, or the buffer was too small
 */
size_t vfs_readlinkat(fd_t dirfd, const char *path, char *buf, size_t size);

/**
 * @brief Create a symbolic link
 *
 * @param path The path to the symbolic link
 * @param target The target of the symbolic link
 * @return true
 * @return false
 */
long vfs_symlink(const char *path, const char *target);

/**
 * @brief Create a directory
 *
 * @param path The path to the directory
 * @return true
 * @return false
 */
PtrResult<void> vfs_mkdir(const char *path);
PtrResult<void> vfs_rmdir(const char *path);

/**
 * @brief Get the content of a directory
 *
 * @param io The io object of a file_t, must be a directory and opened with FILE_OPEN_READ
 * @param buf The buffer to store the directory entries in
 * @param size The size of the buffer
 *
 * @return size_t The number of bytes read, or 0 if the end of the directory was reached
 */
size_t vfs_list_dir(IO *io, void *buf, size_t size);

/**
 * @brief Change the current working directory
 *
 * @param dirfd The file descriptor of the directory to change to
 * @param path The path to the directory, relative to the dirfd
 * @return long 0 on success, or errno on failure
 */
long vfs_chdirat(fd_t dirfd, const char *path);

/**
 * @brief Get the current working directory
 *
 * @param buf The buffer to store the path in
 * @param size The size of the buffer
 * @return ssize_t The size of the path, or 0 if the buffer was too small
 */
ssize_t vfs_getcwd(char *buf, size_t size);

/**
 * @brief Change the permissions of a file
 *
 * @param fd The directory, or a file if FSTATAT_FILE is set
 * @param path The path to the file
 * @param perm The new permissions
 * @param flags fstat_flags flags
 * @return long 0 on success, or errno on failure
 */
long vfs_fchmodat(fd_t fd, const char *path, int perm, int flags);

/**
 * @brief Remove the name of a file, and possibly the file itself
 *
 * @param dirfd The file descriptor of the directory containing the file (or the file itself if AT_EMPTY_PATH is set)
 * @param path The path to the file
 * @return long 0 on success, or errno on failure
 */
long vfs_unlinkat(fd_t dirfd, const char *path);

/**
 * @brief Synchronize a file with the filesystem
 *
 * @param io The io object of a file_t
 * @param sync_metadata Whether to sync the metadata
 * @param start The start of the range to sync
 * @param end The end of the range to sync
 * @return long 0 on success, or errno on failure
 */
long vfs_fsync(IO *io, bool sync_metadata, off_t start, off_t end);

/** @} */
