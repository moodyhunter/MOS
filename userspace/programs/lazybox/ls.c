// SPDX-License-Identifier: GPL-3.0-or-later

#include <fcntl.h>
#include <mos/filesystem/fs_types.h>
#include <mos/syscall/usermode.h>
#include <mos/types.h>
#include <mos_stdio.h>
#include <sys/stat.h>

#define BUFSIZE 4096

static const char *type_to_string[] = {
    [FILE_TYPE_DIRECTORY] = "directory", [FILE_TYPE_REGULAR] = "regular", [FILE_TYPE_CHAR_DEVICE] = "chardev", [FILE_TYPE_BLOCK_DEVICE] = "blockdev",
    [FILE_TYPE_SYMLINK] = "symlink",     [FILE_TYPE_SOCKET] = "socket",   [FILE_TYPE_NAMED_PIPE] = "pipe",     [FILE_TYPE_UNKNOWN] = "unknown",
};

int main(int argc, char *argv[])
{
    if (argc > 2)
    {
        fprintf(stderr, "too many arguments\n");
        fprintf(stderr, "usage: %s [path]\n", argv[0]);
        return 1;
    }

    // argv[1] may contain the path to list
    const char *path = argc > 1 ? argv[1] : ".";

    fd_t dirfd = open(path, OPEN_READ | OPEN_DIR);
    if (dirfd < 0)
    {
        fprintf(stderr, "failed to open directory '%s'\n", path);
        return 1;
    }

    printf("Directory listing of '%s':\n\n", path);
    printf("%-10s %-15s %-5s %-5s %-8s %-10s %-10s\n", "Inode", "Permission", "UID", "GID", "Size", "Type", "Name");

    char buffer[BUFSIZE];
    do
    {
        size_t sz = syscall_vfs_list_dir(dirfd, buffer, BUFSIZE);
        if (sz == 0)
            break;

#define dirent_next(d) ((dir_entry_t *) ((char *) d + d->next_offset))
        for (const dir_entry_t *dirent = (dir_entry_t *) buffer; (char *) dirent < buffer + sz; dirent = dirent_next(dirent))
        {
            file_stat_t statbuf = { 0 };
            if (!lstatat(dirfd, dirent->name, &statbuf))
            {
                fprintf(stderr, "failed to stat '%s'\n", dirent->name);
                continue;
            }

            char perm[10] = "---------";
            file_format_perm(statbuf.perm, perm);
            printf("%-10llu %-15s %-5u %-5u %-8zu %-10s %-10s", dirent->ino, perm, statbuf.uid, statbuf.gid, statbuf.size, type_to_string[dirent->type], dirent->name);

            if (dirent->type == FILE_TYPE_SYMLINK)
            {
                char link[BUFSIZE];
                size_t sz = syscall_vfs_readlinkat(dirfd, dirent->name, link, BUFSIZE);
                bool can_stat = sz > 0 && lstatat(dirfd, link, &statbuf);
                if (can_stat)
                {
                    link[sz] = '\0';
                    printf(" -> %s", link);
                }
                else
                {
                    printf(" -> (broken symlink: '%s')", link);
                }
            }

            puts("");
        }

    } while (true);

    syscall_io_close(dirfd);

    return 0;
}
