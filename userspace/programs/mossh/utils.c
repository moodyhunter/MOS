// SPDX-License-Identifier: GPL-3.0-or-later
#include "mossh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char *locate_program(const char *prog)
{
    if (strchr(prog, '/'))
    {
        struct stat statbuf = { 0 };
        if (!stat(prog, &statbuf))
            if (S_ISLNK(statbuf.st_mode) || S_ISREG(statbuf.st_mode))
                return strdup(prog);

        return NULL;
    }

    for (size_t i = 0; PATH[i]; i++)
    {
        char *path = malloc(strlen(PATH[i]) + strlen(prog) + 2);
        sprintf(path, "%s/%s", PATH[i], prog);

        struct stat statbuf = { 0 };
        if (!stat(path, &statbuf))
            if (S_ISLNK(statbuf.st_mode) || S_ISREG(statbuf.st_mode))
                return path;

        free(path);
    }

    return NULL;
}
