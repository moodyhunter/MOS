// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    // dump the environment variables
    for (char **env = environ; *env != NULL; env++)
    {
        printf("%s\n", *env);
    }

    return 0;
}
