// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

typedef struct _ring_buffer
{
    u8 *data;
    size_t size;
    size_t capacity;
    size_t head; // index of the first element
    size_t tail; // index of the last element
} ring_buffer_t;

ring_buffer_t *ring_buffer_create(size_t capacity);
void ring_buffer_destroy(ring_buffer_t *buffer);

bool ring_buffer_resize(ring_buffer_t *buffer, size_t new_capacity);

size_t ring_buffer_write_back(ring_buffer_t *buffer, const u8 *data, size_t size);
size_t ring_buffer_read_back(ring_buffer_t *buffer, u8 *data, size_t size);

should_inline bool ring_buffer_is_empty(ring_buffer_t *buffer)
{
    return buffer->size == 0;
}

should_inline bool ring_buffer_is_full(ring_buffer_t *buffer)
{
    return buffer->size == buffer->capacity;
}

should_inline u8 ring_buffer_pop_back_byte(ring_buffer_t *buffer)
{
    u8 data = '\0';
    ring_buffer_read_back(buffer, &data, 1);
    return data;
}

should_inline size_t ring_buffer_push_back_byte(ring_buffer_t *buffer, u8 byte)
{
    return ring_buffer_write_back(buffer, &byte, 1);
}
