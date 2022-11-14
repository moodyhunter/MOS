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

    ring_buffer_destroy(rb);
}

MOS_TEST_CASE(ringbuffer_push_pop_back)
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

    ring_buffer_destroy(rb);
}

MOS_TEST_CASE(ringbuffer_push_pop_front)
{
    ring_buffer_t *rb = ring_buffer_create(5);
    size_t written;

    written = ring_buffer_push_front_byte(rb, 'a');
    MOS_TEST_CHECK(rb->size, 1);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 4);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'b');
    MOS_TEST_CHECK(rb->size, 2);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 3);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'c');
    MOS_TEST_CHECK(rb->size, 3);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 2);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'd');
    MOS_TEST_CHECK(rb->size, 4);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 1);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'e');
    MOS_TEST_CHECK(rb->size, 5);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 1);

    char c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(rb->size, 4);
    MOS_TEST_CHECK(rb->head, 4);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(c, 'a');

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(rb->size, 3);
    MOS_TEST_CHECK(rb->head, 3);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(c, 'b');

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(rb->size, 2);
    MOS_TEST_CHECK(rb->head, 2);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(c, 'c');

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(rb->size, 1);
    MOS_TEST_CHECK(rb->head, 1);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(c, 'd');

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(rb->size, 0);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(c, 'e');

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(rb->size, 0);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(c, 0); // empty

    ring_buffer_destroy(rb);
}

MOS_TEST_CASE(ringbuffer_full_and_empty)
{
    ring_buffer_t *rb = ring_buffer_create(1);
    size_t written;
    bool full;
    bool empty;

    full = ring_buffer_is_full(rb);
    MOS_TEST_CHECK(full, false);

    empty = ring_buffer_is_empty(rb);
    MOS_TEST_CHECK(empty, true);

    written = ring_buffer_push_back_byte(rb, 'a');
    MOS_TEST_CHECK(rb->size, 1);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(written, 1);

    full = ring_buffer_is_full(rb);
    MOS_TEST_CHECK(full, true);

    empty = ring_buffer_is_empty(rb);
    MOS_TEST_CHECK(empty, false);

    char c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->size, 0);
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 0);
    MOS_TEST_CHECK(c, 'a');

    full = ring_buffer_is_full(rb);
    MOS_TEST_CHECK(full, false);

    empty = ring_buffer_is_empty(rb);
    MOS_TEST_CHECK(empty, true);
}

MOS_TEST_CASE(ringbuffer_complicated_ops)
{
    ring_buffer_t *rb = ring_buffer_create(10);

    bool full;
    size_t written;
    char c;

    ring_buffer_push_back_byte(rb, '1');
    ring_buffer_push_back_byte(rb, '2');
    ring_buffer_push_back_byte(rb, '3');
    ring_buffer_push_back_byte(rb, '4');
    ring_buffer_push_back_byte(rb, '5');
    ring_buffer_push_back_byte(rb, '6');
    ring_buffer_push_back_byte(rb, '7');
    ring_buffer_push_back_byte(rb, '8');
    ring_buffer_push_back_byte(rb, '9');
    ring_buffer_push_back_byte(rb, '0');

    // |1|2|3|4|5|6|7|8|9|0|

    full = ring_buffer_is_full(rb);
    MOS_TEST_CHECK(full, true);

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(c, '1');

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(c, '2');

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(c, '3');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(c, '0');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(c, '9');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(c, '8');

    // | | | |4|5|6|7| | | |

    written = ring_buffer_push_front_byte(rb, 'a'); // | | |a|4|5|6|7| | | |
    MOS_TEST_CHECK(c, 1);

    written = ring_buffer_push_front_byte(rb, 'b'); // | |b|a|4|5|6|7| | | |
    MOS_TEST_CHECK(rb->head, 1);
    MOS_TEST_CHECK(rb->tail, 6);
    MOS_TEST_CHECK(c, 1);

    written = ring_buffer_push_front_byte(rb, 'c'); // |c|b|a|4|5|6|7| | | |
    MOS_TEST_CHECK(rb->head, 0);
    MOS_TEST_CHECK(rb->tail, 6);
    MOS_TEST_CHECK(c, 1);

    written = ring_buffer_push_front_byte(rb, 'd'); // |c|b|a|4|5|6|7| | |d|
    MOS_TEST_CHECK(rb->head, 9);
    MOS_TEST_CHECK(rb->tail, 6);
    MOS_TEST_CHECK(c, 1);

    written = ring_buffer_push_front_byte(rb, 'e'); // |c|b|a|4|5|6|7| |e|d|
    MOS_TEST_CHECK(rb->head, 8);
    MOS_TEST_CHECK(rb->tail, 6);
    MOS_TEST_CHECK(c, 1);

    written = ring_buffer_push_front_byte(rb, 'f'); // |c|b|a|4|5|6|7|f|e|d|
    MOS_TEST_CHECK(rb->head, 7);
    MOS_TEST_CHECK(rb->tail, 6);
    MOS_TEST_CHECK(c, 1);

    written = ring_buffer_push_front_byte(rb, 'g'); // |c|b|a|4|5|6|7|f|e|d|
    MOS_TEST_CHECK(rb->head, 7);
    MOS_TEST_CHECK(rb->tail, 6);
    MOS_TEST_CHECK(c, 0);

    full = ring_buffer_is_full(rb);
    MOS_TEST_CHECK(full, true);
}
