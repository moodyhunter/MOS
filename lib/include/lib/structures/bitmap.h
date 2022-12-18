// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef u32 bitmap_line_t;
#define BITMAP_LINE_BITS (sizeof(bitmap_line_t) * 8)

bitmap_line_t *bitmap_create(size_t size);
size_t bitmap_nlines(size_t size);
void bitmap_zero(bitmap_line_t *bitmap, size_t bitmap_nlines);
void bitmap_set(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t index);
void bitmap_clear(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t index);
bool bitmap_get(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t index);

size_t bitmap_find_first_free_n(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t n_bits);

should_inline size_t bitmap_find_first_free_n_from(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t n_bits, size_t start_line)
{
    if (start_line >= bitmap_nlines)
        return 0;
    return bitmap_find_first_free_n(bitmap + start_line, bitmap_nlines - start_line, n_bits) + start_line * BITMAP_LINE_BITS;
}
