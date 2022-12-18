// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/bitmap.h"

#include "lib/liballoc.h"
#include "lib/string.h"

bitmap_line_t *bitmap_create(size_t size)
{
    size_t nlines = bitmap_nlines(size);
    bitmap_line_t *bitmap = liballoc_malloc(nlines * sizeof(bitmap_line_t));
    bitmap_zero(bitmap, nlines);
    return bitmap;
}

size_t bitmap_nlines(size_t size)
{
    return ALIGN_UP(size, BITMAP_LINE_BITS) / BITMAP_LINE_BITS;
}

void bitmap_zero(bitmap_line_t *bitmap, size_t bitmap_nlines)
{
    memzero(bitmap, bitmap_nlines * sizeof(bitmap_line_t));
}

void bitmap_set(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t index)
{
    size_t line = index / BITMAP_LINE_BITS;
    size_t bit = index % BITMAP_LINE_BITS;
    if (line >= bitmap_nlines)
        return;
    bitmap[line] |= ((bitmap_line_t) 1 << bit);
}

void bitmap_clear(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t index)
{
    size_t line = index / BITMAP_LINE_BITS;
    size_t bit = index % BITMAP_LINE_BITS;
    if (line >= bitmap_nlines)
        return;
    bitmap[line] &= ~((bitmap_line_t) 1 << bit);
}

bool bitmap_get(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t index)
{
    size_t line = index / BITMAP_LINE_BITS;
    size_t bit = index % BITMAP_LINE_BITS;
    if (line >= bitmap_nlines)
        return false;
    return bitmap[line] & ((bitmap_line_t) 1 << bit);
}

size_t bitmap_find_first_free_n(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t n_bits)
{
    size_t n_free_pages = 0;
    size_t target_starting_bit = 0;
    size_t map_line_i = 0;
    for (size_t i = 0; n_free_pages < n_bits; i++)
    {
        if (i >= bitmap_nlines)
            return 0;

        bitmap_line_t line = bitmap[i];

        if (line == 0)
        {
            n_free_pages += BITMAP_LINE_BITS;
            continue;
        }
        else if (line == (bitmap_line_t) ~0)
        {
            // a full line of used pages
            map_line_i = i + 1;
            n_free_pages = 0;
            target_starting_bit = 0;
            continue;
        }

        for (size_t bit = 0; bit < BITMAP_LINE_BITS; bit++)
        {
            if (!bitmap_get(&line, bitmap_nlines, bit))
            {
                // we have found a free page
                n_free_pages++;
            }
            else
            {
                // this page is used
                if (n_free_pages >= n_bits)
                    // but we have enough free pages
                    break;
                n_free_pages = 0;
                target_starting_bit = bit + 1;
                map_line_i = i;
            }
        }
    }

    return map_line_i * BITMAP_LINE_BITS + target_starting_bit;
}
