// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/file.h"

#include "mos/filesystem/path.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

file_t *file_open(const char *path, file_open_mode mode)
{
    MOS_UNUSED(path);
    MOS_UNUSED(mode);
    pr_warn("file_open: %s (not implemented)", path);
    return 0;
}
