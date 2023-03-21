// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/platform/platform.h>

void x86_64_start_kernel(void)
{
}

mos_platform_info_t x86_64_platform_info = { 0 };
mos_platform_info_t *const platform_info = &x86_64_platform_info;
