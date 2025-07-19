// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 1;
    }

    bool has_error = false;

    for (int i = 1; i < argc; i++)
    {
        if (unlink(argv[i]))
        {
            fprintf(stderr, "failed to remove %s\n", argv[i]);
            has_error = true;
            continue;
        }

        printf("removed %s\n", argv[i]);
    }

    return has_error ? 1 : 0;
}
