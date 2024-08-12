// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        fputs(argv[i], stdout);
        if (i < argc - 1)
            fputs(" ", stdout);
    }

    fputs("\n", stdout);
    return 0;
}
