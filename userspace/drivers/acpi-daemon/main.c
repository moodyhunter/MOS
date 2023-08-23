// SPDX-License-Identifier: GPL-3.0-or-later

#include "lai/core.h"
#include "lai/helpers/pm.h"
#include "lai/helpers/sci.h"

#include <mos/mos_global.h>
#include <mos/platform_syscall.h>
#include <stdio.h>

int main(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    puts("ACPI daemon started.");

    if (syscall_vfs_mkdir("/sys"))
        syscall_vfs_mount("none", "/sys", "sysfs", "defaults");

    syscall_arch_syscall(X86_SYSCALL_IOPL_ENABLE, 0, 0, 0, 0);

    lai_create_namespace();
    lai_enable_acpi(1);

    return 0;
}
