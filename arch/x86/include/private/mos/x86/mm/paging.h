// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/platform/platform.h>
#include <mos/types.h>

void x86_mm_walk_page_table(mm_context_t *handle, ptr_t vaddr_start, size_t n_pages, pgt_iteration_callback_t callback, void *arg);
