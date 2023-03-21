// SPDX-License-Identifier: GPL-3.0-or-later

#include "uefi_boot_info.h"

#include <mos/printk.h>
#include <mos/types.h>

static char cmdline[1024];

static void wide_to_ascii(const u16 *wide, char *ascii)
{
    while (*wide)
        *ascii++ = (char) *wide++;
    *ascii = '\0';
}

asmlinkage void mos_uefi_entry(boot_info_t *boot_info)
{
    wide_to_ascii(boot_info->cmdline, cmdline);
    pr_info("cmdline: %s", cmdline);

    // ...
}
