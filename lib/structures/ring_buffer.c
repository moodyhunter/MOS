// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/ring_buffer.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"

static u8 ring_buffer_get(ring_buffer_t *buffer, size_t index)
{
    return buffer->data[(buffer->head + index) % buffer->size];
}

ring_buffer_t *ring_buffer_create(size_t capacity)
{
    if (capacity == 0)
        return NULL; // forget about it

    ring_buffer_t *rb = kmalloc(sizeof(ring_buffer_t));
    if (!rb)
        return NULL;
    rb->data = kmalloc(capacity);
    if (!rb->data)
    {
        kfree(rb);
        return NULL;
    }
    rb->capacity = capacity;
    rb->size = 0;
    rb->head = 0;
    rb->next_pos = 0;
    return rb;
}

void ring_buffer_destroy(ring_buffer_t *buffer)
{
    kfree(buffer->data);
    kfree(buffer);
}

bool ring_buffer_resize(ring_buffer_t *buffer, size_t new_capacity)
{
    if (new_capacity < buffer->size)
        return false;
    void *new_data = kmalloc(new_capacity);
    if (!new_data)
        return false;
    size_t i = 0;
    while (i < buffer->size)
    {
        ((char *) new_data)[i] = ring_buffer_get(buffer, i);
        i++;
    }
    kfree(buffer->data);
    buffer->data = new_data;
    buffer->capacity = new_capacity;
    buffer->head = 0;
    buffer->next_pos = buffer->size;
    return true;
}

size_t ring_buffer_push_back(ring_buffer_t *buffer, const u8 *data, size_t size)
{
    size_t written = 0;
    while (written < size && buffer->size < buffer->capacity)
    {
        buffer->data[buffer->next_pos] = ((const u8 *) data)[written];
        buffer->next_pos = (buffer->next_pos + 1) % buffer->capacity;
        buffer->size++;
        written++;
    }
    return written;
}

size_t ring_buffer_pop_back(ring_buffer_t *buffer, u8 *data, size_t size)
{
    size_t read = 0;
    while (read < size && buffer->size > 0)
    {
        buffer->next_pos = (buffer->capacity + buffer->next_pos - 1) % buffer->capacity;
        ((u8 *) data)[read] = buffer->data[buffer->next_pos];
        buffer->data[buffer->next_pos] = 0;
        buffer->size--;
        read++;
    }
    return read;
}

size_t ring_buffer_push_front(ring_buffer_t *buffer, const u8 *data, size_t size)
{
    size_t written = 0;
    while (written < size && buffer->size < buffer->capacity)
    {
        buffer->head = (buffer->capacity + buffer->head - 1) % buffer->capacity;
        buffer->data[buffer->head] = ((const u8 *) data)[written];
        buffer->size++;
        written++;
    }
    return written;
}

size_t ring_buffer_pop_front(ring_buffer_t *buffer, u8 *data, size_t size)
{
    size_t read = 0;
    while (read < size && buffer->size > 0)
    {
        ((u8 *) data)[read] = buffer->data[buffer->head];
        buffer->data[buffer->head] = 0;
        buffer->head = (buffer->capacity + buffer->head + 1) % buffer->capacity;
        buffer->size--;
        read++;
    }
    return read;
}
