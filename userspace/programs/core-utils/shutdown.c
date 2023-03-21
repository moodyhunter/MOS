// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    syscall_poweroff(false, MOS_FOURCC('G', 'B', 'y', 'e'));
    return 0;
}
