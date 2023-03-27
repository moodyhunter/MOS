// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mos_global.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0)
        printf("Usage: true\n");

    return 0;
}
