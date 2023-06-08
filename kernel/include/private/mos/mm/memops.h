// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

typedef struct
{
    ptr_t v;
    pfn_t p;
} mm_get_one_page_result_t;

mm_get_one_page_result_t mm_get_one_zero_page(void);
