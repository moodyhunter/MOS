// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

#define dirent_next(d) ((struct dirent *) ((char *) d + d->d_reclen))
static size_t depth = 1;

static void print_entry(const struct dirent *dirent)
{
    for (size_t i = 0; i < depth; i++)
        printf("    ");
    printf("%s\n", dirent->d_name);
}

static void do_tree(void)
{
    const fd_t dirfd = open(".", OPEN_READ | OPEN_DIR);
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

        for (const struct dirent *dirent = (struct dirent *) buffer; (char *) dirent < buffer + sz; dirent = dirent_next(dirent))
        {
            if (dirent->d_type == FILE_TYPE_DIRECTORY)
            {
                if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
                    continue;

                print_entry(dirent);
                depth++;
                chdir(dirent->d_name);
                do_tree();
                chdir("..");
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
    if (argc > 2)
    {
        fprintf(stderr, "too many arguments\n");
        fprintf(stderr, "usage: %s [path]\n", argv[0]);
        return 1;
    }

    // argv[1] may contain the path to list
    const char *path = argc > 1 ? argv[1] : ".";

    if (chdir(path) != 0)
    {
        fprintf(stderr, "failed to chdir to '%s'\n", path);
        return 1;
    }

    printf("%s\n", path);
    do_tree();
    return 0;
}
