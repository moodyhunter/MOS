// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/ring_buffer.h"
#include "test_engine.h"

#include <stddef.h>

MOS_TEST_CASE(ringbuffer_creation_and_destruction)
{
    ring_buffer_t *rb = ring_buffer_create(10);

    MOS_TEST_CHECK(rb != NULL, true);
    MOS_TEST_CHECK(rb->capacity, 10);
    MOS_TEST_CHECK(rb->size, 0);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);

    ring_buffer_destroy(rb);

    rb = ring_buffer_create(0);
    MOS_TEST_CHECK(rb, NULL);
}

MOS_TEST_CASE(ringbuffer_put_and_get)
{
    ring_buffer_t *rb = ring_buffer_create(5);
    size_t written;

    written = ring_buffer_push_back_byte(rb, 'a');
    MOS_TEST_CHECK(rb->size, 1);
    MOS_TEST_CHECK(rb->head, 1);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'b');
    MOS_TEST_CHECK(rb->size, 2);
    MOS_TEST_CHECK(rb->head, 2);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'c');
    MOS_TEST_CHECK(rb->size, 3);
    MOS_TEST_CHECK(rb->head, 3);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'd');
    MOS_TEST_CHECK(rb->size, 4);
    MOS_TEST_CHECK(rb->head, 4);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'e');
    MOS_TEST_CHECK(rb->size, 5);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'f');
    MOS_TEST_CHECK(rb->size, 5);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 0); // full

    char c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 4);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 1);
    MOS_TEST_CHECK(c, 'a');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 3);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 2);
    MOS_TEST_CHECK(c, 'b');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 2);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 3);
    MOS_TEST_CHECK(c, 'c');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 1);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 4);
    MOS_TEST_CHECK(c, 'd');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 0);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(c, 'e');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 0);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(c, 0); // empty
}

MOS_TEST_CASE(ringbuffer_put_and_get_with_wrap)
{
    ring_buffer_t *rb = ring_buffer_create(5);
    size_t written;

    written = ring_buffer_push_back_byte(rb, 'a');
    MOS_TEST_CHECK(rb->size, 1);
    MOS_TEST_CHECK(rb->head, 1);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'b');
    MOS_TEST_CHECK(rb->size, 2);
    MOS_TEST_CHECK(rb->head, 2);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'c');
    MOS_TEST_CHECK(rb->size, 3);
    MOS_TEST_CHECK(rb->head, 3);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'd');
    MOS_TEST_CHECK(rb->size, 4);
    MOS_TEST_CHECK(rb->head, 4);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'e');
    MOS_TEST_CHECK(rb->size, 5);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 1);

    char c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 4);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 1);
    MOS_TEST_CHECK(c, 'a');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 3);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 2);
    MOS_TEST_CHECK(c, 'b');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 2);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 3);
    MOS_TEST_CHECK(c, 'c');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 1);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 4);
    MOS_TEST_CHECK(c, 'd');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 0);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(c, 'e');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 0);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(c, 0); // empty
}
