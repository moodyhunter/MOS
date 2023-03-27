// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/filesystem/fs_types.h>
#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define dirent_next(d) ((dir_entry_t *) ((char *) d + d->next_offset))
static size_t depth = 1;

static void print_entry(const dir_entry_t *dirent)
{
    for (size_t i = 0; i < depth; i++)
        printf("    ");
    printf("%s\n", dirent->name);
}

static void do_tree(void)
{
    const fd_t dirfd = syscall_vfs_open(".", OPEN_READ | OPEN_DIR);
    if (dirfd < 0)
    {
        fprintf(stderr, "failed to open directory\n");
        return;
    }

    char buffer[1024];
    do
    {
        size_t sz = syscall_vfs_list_dir(dirfd, buffer, sizeof(buffer));
        if (sz == 0)
            break;

        for (const dir_entry_t *dirent = (dir_entry_t *) buffer; (char *) dirent < buffer + sz; dirent = dirent_next(dirent))
        {
            if (dirent->type == FILE_TYPE_DIRECTORY)
            {
                if (strcmp(dirent->name, ".") == 0 || strcmp(dirent->name, "..") == 0)
                    continue;

                print_entry(dirent);
                depth++;
                syscall_vfs_chdir(dirent->name);
                do_tree();
                syscall_vfs_chdir("..");
                depth--;
            }
            else
            {
                print_entry(dirent);
            }
        }

    } while (true);

    syscall_io_close(dirfd);
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
    {
        fputs("usage: tree <dir>\n", stderr);
        return 1;
    }

    if (!syscall_vfs_chdir(argv[1]))
    {
        fprintf(stderr, "failed to chdir to '%s'\n", argv[1]);
        return 1;
    }

    printf("%s\n", argv[1]);
    do_tree();
    return 0;
}
