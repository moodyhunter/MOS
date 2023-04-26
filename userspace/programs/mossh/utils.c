// SPDX-License-Identifier: GPL-3.0-or-later
#include "mossh.h"

#include <mos/filesystem/fs_types.h>
#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char *locate_program(const char *prog)
{
    if (strchr(prog, '/'))
    {
        file_stat_t statbuf = { 0 };
        if (stat(prog, &statbuf))
        {
            if (statbuf.type == FILE_TYPE_SYMLINK || statbuf.type == FILE_TYPE_REGULAR)
                return strdup(prog);

            fprintf(stderr, "Not a file: '%s'\n", prog);
        }

        return NULL;
    }

    for (size_t i = 0; PATH[i]; i++)
    {
        char *path = malloc(strlen(PATH[i]) + strlen(prog) + 2);
        sprintf(path, "%s/%s", PATH[i], prog);

        file_stat_t statbuf = { 0 };
        if (stat(path, &statbuf))
        {
            if (statbuf.type == FILE_TYPE_SYMLINK || statbuf.type == FILE_TYPE_REGULAR)
                return path;

            fprintf(stderr, "Not a file: '%s'\n", path);
        }

        free(path);
    }

    return NULL;
}
