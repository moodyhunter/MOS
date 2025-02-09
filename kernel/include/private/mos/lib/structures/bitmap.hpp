// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/moslib_global.hpp>
#include <mos/types.hpp>

typedef u32 bitmap_line_t;

#define BITMAP_LINE_BITS        (sizeof(bitmap_line_t) * 8)
#define BITMAP_LINE_COUNT(size) (ALIGN_UP(size, BITMAP_LINE_BITS) / BITMAP_LINE_BITS)

MOSAPI bitmap_line_t *bitmap_create(size_t size);

MOSAPI void bitmap_zero(bitmap_line_t *bitmap, size_t bitmap_nlines);
MOSAPI bool bitmap_set(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t index);
MOSAPI bool bitmap_clear(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t index);
MOSAPI void bitmap_set_range(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t start, size_t end, bool value);
MOSAPI bool bitmap_get(const bitmap_line_t *bitmap, size_t bitmap_nlines, size_t index);

MOSAPI size_t bitmap_find_first_free_n(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t begin_bit, size_t n_bits);
