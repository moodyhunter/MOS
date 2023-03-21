// SPDX-License-Identifier: GPL-3.0-or-later

#include "libuserspace.h"

#include <mos/filesystem/fs_types.h>
#include <mos/syscall/usermode.h>
#include <mos/types.h>

#define BUFSIZE 256

static const char *type_to_string[] = {
    [FILE_TYPE_DIRECTORY] = "directory", [FILE_TYPE_REGULAR] = "regular", [FILE_TYPE_CHAR_DEVICE] = "char", [FILE_TYPE_BLOCK_DEVICE] = "block",
    [FILE_TYPE_SYMLINK] = "symlink",     [FILE_TYPE_SOCKET] = "socket",   [FILE_TYPE_NAMED_PIPE] = "pipe",  [FILE_TYPE_UNKNOWN] = "unknown",
};

int main(int argc, char *argv[])
{
    // argv[1] may contain the path to list
    const char *path = argc > 1 ? argv[1] : ".";

    fd_t dirfd = syscall_vfs_open(path, OPEN_READ | OPEN_DIR);
    if (dirfd < 0)
    {
        dprintf(stderr, "failed to open directory '%s'", path);
        return 1;
    }

    char buffer[BUFSIZE];

    printf("Directory listing of '%s':\n", path);
    printf("\n");
    printf("%-10s %-10s %-10s\n", "Inode", "Type", "Name");

    do
    {
        size_t sz = syscall_vfs_list_dir(dirfd, buffer, BUFSIZE);
        if (sz == 0)
            break;

#define dirent_next(d) ((dir_entry_t *) ((char *) d + d->next_offset))

        for (const dir_entry_t *dirent = (dir_entry_t *) buffer; (char *) dirent < buffer + sz; dirent = dirent_next(dirent))
        {
            printf("%-10llu %-10s %-10s\n", dirent->ino, type_to_string[dirent->type], dirent->name);
        }
    } while (true);

    syscall_io_close(dirfd);

    return 0;
}
