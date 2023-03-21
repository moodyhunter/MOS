// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mm/paging/paging.h>
#include <mos/tasks/task_types.h>
#include <mos/types.h>

uintptr_t mmap_anonymous(uintptr_t hint_addr, mmap_flags_t flags, vm_flags vm_flags, size_t n_pages);
uintptr_t mmap_file(uintptr_t hint_addr, mmap_flags_t flags, vm_flags vm_flags, size_t size, io_t *io, off_t offset);

bool munmap(uintptr_t addr, size_t size);
