// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <mos/allocator.hpp>
#include <mos/lib/structures/ring_buffer.hpp>
#include <mos/moslib_global.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

static u8 ring_buffer_get(u8 *data, ring_buffer_pos_t *pos, size_t index)
{
    return data[(pos->head + index) % pos->count];
}

ring_buffer_t *ring_buffer_create(size_t capacity)
{
    if (capacity == 0)
        return NULL; // forget about it

    ring_buffer_t *rb = mos::create<ring_buffer_t>();
    if (!rb)
        return NULL;
    rb->data = (u8 *) kcalloc<u8>(capacity);
    if (!rb->data)
    {
        delete rb;
        return NULL;
    }
    ring_buffer_pos_init(&rb->pos, capacity);
    return rb;
}

ring_buffer_t *ring_buffer_create_at(void *data, size_t capacity)
{
    if (capacity == 0)
        return NULL; // forget about it

    ring_buffer_t *rb = mos::create<ring_buffer_t>();
    if (!rb)
        return NULL;
    rb->data = (u8 *) data;
    ring_buffer_pos_init(&rb->pos, capacity);
    return rb;
}

void ring_buffer_pos_init(ring_buffer_pos_t *pos, size_t capacity)
{
    pos->capacity = capacity;
    pos->count = 0;
    pos->head = 0;
    pos->next_pos = 0;
}

void ring_buffer_destroy(ring_buffer_t *buffer)
{
    kfree(buffer->data);
    delete buffer;
}

bool ring_buffer_resize(ring_buffer_t *buffer, size_t new_capacity)
{
    if (new_capacity < buffer->pos.count)
        return false;
    void *new_data = kcalloc<char>(new_capacity);
    if (!new_data)
        return false;
    size_t i = 0;
    while (i < buffer->pos.count)
    {
        ((char *) new_data)[i] = ring_buffer_get(buffer->data, &buffer->pos, i);
        i++;
    }

    kfree(buffer->data);
    buffer->data = (u8 *) new_data;
    buffer->pos.capacity = new_capacity;
    buffer->pos.head = 0;
    buffer->pos.next_pos = buffer->pos.count;
    return true;
}

size_t ring_buffer_pos_push_back(u8 *data, ring_buffer_pos_t *pos, const u8 *target, size_t size)
{
    if (pos->count + size > pos->capacity)
        size = pos->capacity - pos->count;

    size_t first_part_i = pos->next_pos;
    size_t first_part_size = std::min(size, pos->capacity - pos->next_pos);
    size_t second_part = size - first_part_size;
    memcpy(data + first_part_i, target, first_part_size);
    memcpy(data, target + first_part_size, second_part);
    pos->next_pos = (pos->next_pos + size) % pos->capacity;
    pos->count += size;
    return size;
}

size_t ring_buffer_pos_pop_back(u8 *data, ring_buffer_pos_t *pos, u8 *target, size_t size)
{
    if (size > pos->count)
        size = pos->count;

    size_t first_part_i = (pos->capacity + pos->next_pos - size) % pos->capacity;
    size_t first_part_size = std::min(size, pos->capacity - first_part_i);

    size_t second_part_i = 0;
    size_t second_part_size = size - first_part_size;

    memcpy(target, data + first_part_i, first_part_size);
    memcpy(target + first_part_size, data + second_part_i, second_part_size);

    pos->next_pos = (pos->capacity + pos->next_pos - size) % pos->capacity;
    pos->count -= size;

    return size;
}

size_t ring_buffer_pos_push_front(u8 *data, ring_buffer_pos_t *pos, const u8 *target, size_t size)
{
    if (pos->count + size > pos->capacity)
        return 0;

    size_t first_part_i = (pos->capacity + pos->head - size) % pos->capacity;
    size_t first_part_size = std::min(size, pos->capacity - first_part_i);

    size_t second_part_i = 0;
    size_t second_part_size = size - first_part_size;

    memcpy(data + first_part_i, target, first_part_size);
    memcpy(data + second_part_i, target + first_part_size, second_part_size);

    pos->head = (pos->capacity + pos->head - size) % pos->capacity;
    pos->count += size;
    return size;
}

size_t ring_buffer_pos_pop_front(u8 *data, ring_buffer_pos_t *pos, u8 *target, size_t size)
{
    if (size > pos->count)
        size = pos->count;

    size_t first_part_i = pos->head;
    size_t first_part_size = std::min(size, pos->capacity - first_part_i);

    size_t second_part_i = 0;
    size_t second_part_size = size - first_part_size;

    memcpy(target, data + first_part_i, first_part_size);
    memcpy(target + first_part_size, data + second_part_i, second_part_size);

    pos->head = (pos->head + size) % pos->capacity;
    pos->count -= size;
    return size;
}
