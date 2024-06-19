// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/moslib_global.h>
#include <mos/types.h>

/**
 * @defgroup ringbuffer libs.RingBuffer
 * @ingroup libs
 * @brief A ring buffer.
 * @{
 */

// A position-only ring buffer specifier.
typedef struct _ring_buffer_pos
{
    size_t count;
    size_t capacity;
    size_t head;     // index of the first element
    size_t next_pos; // index of the next element to be inserted
} ring_buffer_pos_t;

// A managed ring buffer.
typedef struct _ring_buffer
{
    u8 *data;
    ring_buffer_pos_t pos;
} ring_buffer_t;

MOSAPI ring_buffer_t *ring_buffer_create(size_t capacity);
MOSAPI ring_buffer_t *ring_buffer_create_at(void *data, size_t capacity);
MOSAPI void ring_buffer_pos_init(ring_buffer_pos_t *pos, size_t capacity);
MOSAPI void ring_buffer_destroy(ring_buffer_t *buffer);

MOSAPI bool ring_buffer_resize(ring_buffer_t *buffer, size_t new_capacity);

MOSAPI size_t ring_buffer_pos_push_back(u8 *buffer, ring_buffer_pos_t *pos, const u8 *data, size_t size);
MOSAPI size_t ring_buffer_pos_pop_back(u8 *buffer, ring_buffer_pos_t *pos, u8 *buf, size_t size);

MOSAPI size_t ring_buffer_pos_push_front(u8 *buffer, ring_buffer_pos_t *pos, const u8 *data, size_t size);
MOSAPI size_t ring_buffer_pos_pop_front(u8 *buffer, ring_buffer_pos_t *pos, u8 *buf, size_t size);

should_inline u8 ring_buffer_pos_pop_back_byte(u8 *buffer, ring_buffer_pos_t *pos)
{
    u8 data = '\0';
    ring_buffer_pos_pop_back(buffer, pos, &data, 1);
    return data;
}

should_inline u8 ring_buffer_pos_pop_front_byte(u8 *buffer, ring_buffer_pos_t *pos)
{
    u8 data = '\0';
    ring_buffer_pos_pop_front(buffer, pos, &data, 1);
    return data;
}

// clang-format off
should_inline bool ring_buffer_pos_is_full(ring_buffer_pos_t *pos) { return pos->count == pos->capacity; }
should_inline bool ring_buffer_pos_is_empty(ring_buffer_pos_t *pos) { return pos->count == 0; }

should_inline size_t ring_buffer_pos_push_back_byte(u8 *buffer, ring_buffer_pos_t *pos, u8 data) { return ring_buffer_pos_push_back(buffer, pos, &data, 1); }
should_inline size_t ring_buffer_pos_push_front_byte(u8 *buffer, ring_buffer_pos_t *pos,  u8 data) { return ring_buffer_pos_push_front(buffer, pos, &data, 1); }

// ring_buffer_t wrapper functions.

should_inline bool ring_buffer_is_full(ring_buffer_t *buffer) { return ring_buffer_pos_is_full(&buffer->pos); }
should_inline bool ring_buffer_is_empty(ring_buffer_t *buffer) { return ring_buffer_pos_is_empty(&buffer->pos); }

should_inline size_t ring_buffer_push_back(ring_buffer_t *buffer, const u8 *data, size_t size) { return ring_buffer_pos_push_back(buffer->data, &buffer->pos, data, size); }
should_inline size_t ring_buffer_pop_back(ring_buffer_t *buffer, u8 *buf, size_t size) { return ring_buffer_pos_pop_back(buffer->data, &buffer->pos, buf, size); }

should_inline size_t ring_buffer_push_front(ring_buffer_t *buffer, const u8 *data, size_t size) { return ring_buffer_pos_push_front(buffer->data, &buffer->pos, data, size); }
should_inline size_t ring_buffer_pop_front(ring_buffer_t *buffer, u8 *buf, size_t size) { return ring_buffer_pos_pop_front(buffer->data, &buffer->pos, buf, size); }

should_inline size_t ring_buffer_push_front_byte(ring_buffer_t *buffer, u8 byte) { return ring_buffer_pos_push_front_byte(buffer->data, &buffer->pos, byte); }

should_inline size_t ring_buffer_push_back_byte(ring_buffer_t *buffer, u8 byte) { return ring_buffer_pos_push_back_byte(buffer->data, &buffer->pos, byte); }
should_inline u8 ring_buffer_pop_back_byte(ring_buffer_t *buffer) { return ring_buffer_pos_pop_back_byte(buffer->data, &buffer->pos); }
should_inline u8 ring_buffer_pop_front_byte(ring_buffer_t *buffer) { return ring_buffer_pos_pop_front_byte(buffer->data, &buffer->pos); }
// clang-format on
/** @} */
