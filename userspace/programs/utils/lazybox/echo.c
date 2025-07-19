// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

auto const ARG_N = "-n";

int main(int argc, char **argv)
{
    bool newline = true;
    if (argc > 1 && strcmp(argv[1], ARG_N) == 0)
    {
        // skip the first argument
        argc--;
        argv++;
        newline = false;
    }

    for (int i = 1; i < argc; i++)
    {
        fputs(argv[i], stdout);
        if (i < argc - 1)
            fputs(" ", stdout);
    }

    if (newline)
        fputs("\n", stdout);
    return 0;
}
