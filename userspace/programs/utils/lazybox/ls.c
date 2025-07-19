// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

#define BUFSIZE 4096

static const char *type_to_string[] = {
    [FILE_TYPE_DIRECTORY] = "dir",   [FILE_TYPE_REGULAR] = "file",  [FILE_TYPE_CHAR_DEVICE] = "char", [FILE_TYPE_BLOCK_DEVICE] = "block",
    [FILE_TYPE_SYMLINK] = "symlink", [FILE_TYPE_SOCKET] = "socket", [FILE_TYPE_NAMED_PIPE] = "pipe",  [FILE_TYPE_UNKNOWN] = "unknown",
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
    printf("%-10s %-15s %-5s %-5s %-15s %-10s %-10s\n", "Inode", "Permission", "UID", "GID", "Size", "Type", "Name");

    char buffer[BUFSIZE];
    do
    {
        size_t sz = syscall_vfs_list_dir(dirfd, buffer, BUFSIZE);
        if (sz == 0)
            break;

#define dirent_next(d) ((struct dirent *) ((char *) d + d->d_reclen))
        for (const struct dirent *dirent = (struct dirent *) buffer; (char *) dirent < buffer + sz; dirent = dirent_next(dirent))
        {
            file_stat_t statbuf = { 0 };
            if (!lstatat(dirfd, dirent->d_name, &statbuf))
            {
                fprintf(stderr, "failed to stat '%s'\n", dirent->d_name);
                continue;
            }

            char perm[10] = "---------";
            file_format_perm(statbuf.perm, perm);
            char namebuf[256] = { 0 };
            snprintf(namebuf, sizeof(namebuf), "%s%s", dirent->d_name, dirent->d_type == FILE_TYPE_DIRECTORY ? "/" : "");

            printf("%-10lu %-15s %-5u %-5u %-15zu %-10s %-10s", dirent->d_ino, perm, statbuf.uid, statbuf.gid, statbuf.size, type_to_string[dirent->d_type], namebuf);

            if (dirent->d_type == FILE_TYPE_SYMLINK)
            {
                char link[BUFSIZE] = { 0 };
                size_t sz = syscall_vfs_readlinkat(dirfd, dirent->d_name, link, BUFSIZE);
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
