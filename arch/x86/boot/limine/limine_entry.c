// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/printk.h>
#include <mos/types.h>

static char cmdline[1024];

asmlinkage void mos_limine_entry(void)
{
    pr_info("cmdline: %s", cmdline);
}
