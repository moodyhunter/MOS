// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/lib/structures/bitmap.h>
#include <mos_stdlib.h>
#include <mos_string.h>

bitmap_line_t *bitmap_create(size_t size)
{
    size_t nlines = BITMAP_LINE_COUNT(size);
    bitmap_line_t *bitmap = kmalloc(nlines * sizeof(bitmap_line_t));
    bitmap_zero(bitmap, nlines);
    return bitmap;
}

void bitmap_zero(bitmap_line_t *bitmap, size_t bitmap_nlines)
{
    memzero(bitmap, bitmap_nlines * sizeof(bitmap_line_t));
}

bool bitmap_set(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t index)
{
    size_t line = index / BITMAP_LINE_BITS;
    size_t bit = index % BITMAP_LINE_BITS;
    if (line >= bitmap_nlines)
        return false;

    bool original = bitmap_get(bitmap, bitmap_nlines, index);
    bitmap[line] |= ((bitmap_line_t) 1 << bit);

    return original == false;
}

bool bitmap_clear(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t index)
{
    size_t line = index / BITMAP_LINE_BITS;
    size_t bit = index % BITMAP_LINE_BITS;
    if (line >= bitmap_nlines)
        return false;

    bool original = bitmap_get(bitmap, bitmap_nlines, index);
    bitmap[line] &= ~((bitmap_line_t) 1 << bit);

    return original == true;
}

bool bitmap_get(const bitmap_line_t *bitmap, size_t bitmap_nlines, size_t index)
{
    size_t line = index / BITMAP_LINE_BITS;
    size_t bit = index % BITMAP_LINE_BITS;
    if (line >= bitmap_nlines)
        return false;
    return bitmap[line] & ((bitmap_line_t) 1 << bit);
}

size_t bitmap_find_first_free_n(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t begin_bit, size_t n_bits)
{
    size_t target_starting_line = begin_bit / BITMAP_LINE_BITS;
    size_t target_starting_bit = begin_bit % BITMAP_LINE_BITS;
    size_t free_bits = 0;

    for (size_t line_i = target_starting_line; free_bits < n_bits; line_i++)
    {
        if (line_i >= bitmap_nlines)
            return -1;

        bitmap_line_t line = bitmap[line_i];

        if (line == 0)
        {
            free_bits += BITMAP_LINE_BITS;
            continue;
        }
        else if (line == (bitmap_line_t) ~0)
        {
            // a full line of occupied bits
            target_starting_line = line_i + 1;
            free_bits = 0;
            target_starting_bit = 0;
            continue;
        }

        for (size_t bit = 0; bit < BITMAP_LINE_BITS; bit++)
        {
            if (free_bits >= n_bits)
                break;

            if (!bitmap_get(&line, bitmap_nlines, bit))
            {
                // we have found a free line
                free_bits++;
            }
            else
            {
                // this bit is occupied
                free_bits = 0;
                target_starting_bit = bit + 1;
                target_starting_line = line_i;
            }
        }
    }

    return target_starting_line * BITMAP_LINE_BITS + target_starting_bit;
}

void bitmap_set_range(bitmap_line_t *bitmap, size_t bitmap_nlines, size_t start_bit, size_t end_bit, bool value)
{
    size_t start_line = start_bit / BITMAP_LINE_BITS;
    size_t start_bit_in_line = start_bit % BITMAP_LINE_BITS;
    size_t end_line = end_bit / BITMAP_LINE_BITS;
    size_t end_bit_in_line = end_bit % BITMAP_LINE_BITS;

    if (start_line >= bitmap_nlines || end_line >= bitmap_nlines)
        return;

    if (start_line == end_line)
    {
        bitmap_line_t line = bitmap[start_line];
        bitmap_line_t mask = ((bitmap_line_t) 1 << (end_bit_in_line - start_bit_in_line + 1)) - 1;
        mask <<= start_bit_in_line;
        if (value)
            line |= mask;
        else
            line &= ~mask;
        bitmap[start_line] = line;
        return;
    }

    bitmap_line_t line = bitmap[start_line];
    bitmap_line_t mask = ((bitmap_line_t) 1 << (BITMAP_LINE_BITS - start_bit_in_line)) - 1;
    mask <<= start_bit_in_line;
    if (value)
        line |= mask;
    else
        line &= ~mask;
    bitmap[start_line] = line;

    for (size_t i = start_line + 1; i < end_line; i++)
    {
        if (value)
            bitmap[i] = (bitmap_line_t) ~0;
        else
            bitmap[i] = 0;
    }

    line = bitmap[end_line];
    mask = ((bitmap_line_t) 1 << (end_bit_in_line + 1)) - 1;
    if (value)
        line |= mask;
    else
        line &= ~mask;
    bitmap[end_line] = line;
}
