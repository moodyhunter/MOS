// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 1;
    }

    if (!unlink(argv[1]))
    {
        fprintf(stderr, "failed to remove file\n");
        return 1;
    }

    printf("removed %s\n", argv[1]);
    return 0;
}
