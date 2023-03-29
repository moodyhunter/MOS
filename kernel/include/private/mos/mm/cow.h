// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/platform/platform.h>

vmblock_t mm_make_process_map_cow(paging_handle_t from, ptr_t fvaddr, paging_handle_t to, ptr_t tvaddr, size_t npages, vm_flags flags);
bool cow_handle_page_fault(ptr_t fault_addr, bool present, bool is_write, bool is_user, bool is_exec);
