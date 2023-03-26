// SPDX-License-Identifier: GPL-3.0-or-later
#include "mossh.h"

#include <mos/filesystem/fs_types.h>
#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *locate_program(const char *prog)
{
    if (strchr(prog, '/'))
    {
        file_stat_t stat = { 0 };
        if (syscall_vfs_stat(prog, &stat))
        {
            if (stat.type == FILE_TYPE_SYMLINK || stat.type == FILE_TYPE_REGULAR)
                return strdup(prog);

            dprintf(stderr, "Not a file: '%s'\n", prog);
        }

        return NULL;
    }

    for (size_t i = 0; PATH[i]; i++)
    {
        char *path = malloc(strlen(PATH[i]) + strlen(prog) + 2);
        sprintf(path, "%s/%s", PATH[i], prog);

        file_stat_t stat = { 0 };
        if (syscall_vfs_stat(path, &stat))
        {
            if (stat.type == FILE_TYPE_SYMLINK || stat.type == FILE_TYPE_REGULAR)
                return path;

            dprintf(stderr, "Not a file: '%s'\n", path);
        }

        free(path);
    }

    return NULL;
}
