// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/platform/platform.h"

static mos_platform_info_t riscv64_platform_info;
mos_platform_info_t *const platform_info = &riscv64_platform_info;

static mos_platform_info_t riscv64_platform_info = {
    .num_cpus = 1,
};

void platform_startup_early()
{
}

void platform_startup_mm()
{
}

void platform_startup_late()
{
}
