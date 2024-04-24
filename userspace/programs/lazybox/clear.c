// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    printf("\033[2J\033[H");
    return 0;
}
