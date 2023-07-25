// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/physical/pmm.h"

#include <mos/types.h>

phyframe_t *mm_get_free_page(void);
phyframe_t *mm_get_free_page_raw(void);

phyframe_t *mm_get_free_pages(size_t npages);

void mm_unref_pages(phyframe_t *frame, size_t npages);
