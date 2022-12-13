// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/ring_buffer.h"
#include "test_engine.h"

#include <stddef.h>

MOS_TEST_CASE(ringbuffer_creation_and_destruction)
{
    ring_buffer_t *rb = ring_buffer_create(10);

    MOS_TEST_CHECK(rb != NULL, true);
    MOS_TEST_CHECK(rb->pos.capacity, 10);
    MOS_TEST_CHECK(rb->pos.size, 0);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);

    ring_buffer_destroy(rb);

    rb = ring_buffer_create(0);
    MOS_TEST_CHECK(rb, NULL);
}

MOS_TEST_CASE(ringbuffer_put_and_get)
{
    ring_buffer_t *rb = ring_buffer_create(5);
    size_t written;

    written = ring_buffer_push_back_byte(rb, 'a');
    MOS_TEST_CHECK(rb->pos.size, 1);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 1);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'b');
    MOS_TEST_CHECK(rb->pos.size, 2);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 2);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'c');
    MOS_TEST_CHECK(rb->pos.size, 3);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 3);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'd');
    MOS_TEST_CHECK(rb->pos.size, 4);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 4);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'e');
    MOS_TEST_CHECK(rb->pos.size, 5);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'f');
    MOS_TEST_CHECK(rb->pos.size, 5);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(written, 0); // full

    char c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 4);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 4);
    MOS_TEST_CHECK(c, 'e');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 3);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 3);
    MOS_TEST_CHECK(c, 'd');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 2);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 2);
    MOS_TEST_CHECK(c, 'c');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 1);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 1);
    MOS_TEST_CHECK(c, 'b');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 0);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(c, 'a');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 0);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(c, 0); // empty

    ring_buffer_destroy(rb);
}

MOS_TEST_CASE(ringbuffer_push_pop_back)
{
    ring_buffer_t *rb = ring_buffer_create(5);
    size_t written;

    written = ring_buffer_push_back_byte(rb, 'a');
    MOS_TEST_CHECK(rb->pos.size, 1);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 1);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'b');
    MOS_TEST_CHECK(rb->pos.size, 2);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 2);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'c');
    MOS_TEST_CHECK(rb->pos.size, 3);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 3);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'd');
    MOS_TEST_CHECK(rb->pos.size, 4);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 4);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_back_byte(rb, 'e');
    MOS_TEST_CHECK(rb->pos.size, 5);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(written, 1);

    char c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 4);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 4);
    MOS_TEST_CHECK(c, 'e');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 3);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 3);
    MOS_TEST_CHECK(c, 'd');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 2);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 2);
    MOS_TEST_CHECK(c, 'c');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 1);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 1);
    MOS_TEST_CHECK(c, 'b');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 0);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(c, 'a');

    c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 0);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(c, 0); // empty

    ring_buffer_destroy(rb);
}

MOS_TEST_CASE(ringbuffer_push_pop_front)
{
    ring_buffer_t *rb = ring_buffer_create(5);
    size_t written;

    written = ring_buffer_push_front_byte(rb, 'a');
    MOS_TEST_CHECK(rb->pos.size, 1);
    MOS_TEST_CHECK(rb->pos.head, 4);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'b');
    MOS_TEST_CHECK(rb->pos.size, 2);
    MOS_TEST_CHECK(rb->pos.head, 3);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'c');
    MOS_TEST_CHECK(rb->pos.size, 3);
    MOS_TEST_CHECK(rb->pos.head, 2);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'd');
    MOS_TEST_CHECK(rb->pos.size, 4);
    MOS_TEST_CHECK(rb->pos.head, 1);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'e');
    MOS_TEST_CHECK(rb->pos.size, 5);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(written, 1);

    char c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 4);
    MOS_TEST_CHECK(rb->pos.head, 1);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(c, 'e');

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 3);
    MOS_TEST_CHECK(rb->pos.head, 2);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(c, 'd');

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 2);
    MOS_TEST_CHECK(rb->pos.head, 3);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(c, 'c');

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 1);
    MOS_TEST_CHECK(rb->pos.head, 4);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(c, 'b');

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 0);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(c, 'a');

    c = ring_buffer_pop_front_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 0);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
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
    MOS_TEST_CHECK(rb->pos.size, 1);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(written, 1);

    full = ring_buffer_is_full(rb);
    MOS_TEST_CHECK(full, true);

    empty = ring_buffer_is_empty(rb);
    MOS_TEST_CHECK(empty, false);

    char c = ring_buffer_pop_back_byte(rb);
    MOS_TEST_CHECK(rb->pos.size, 0);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
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
    MOS_TEST_CHECK(rb->pos.size, 5);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 5);

    ring_buffer_push_back_byte(rb, '6');
    ring_buffer_push_back_byte(rb, '7');
    ring_buffer_push_back_byte(rb, '8');
    ring_buffer_push_back_byte(rb, '9');
    ring_buffer_push_back_byte(rb, '0');
    MOS_TEST_CHECK(rb->pos.size, 10);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);

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
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'b'); // | |b|a|4|5|6|7| | | |
    MOS_TEST_CHECK(rb->pos.head, 1);
    MOS_TEST_CHECK(rb->pos.next_pos, 7);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'c'); // |c|b|a|4|5|6|7| | | |
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 7);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'd'); // |c|b|a|4|5|6|7| | |d|
    MOS_TEST_CHECK(rb->pos.head, 9);
    MOS_TEST_CHECK(rb->pos.next_pos, 7);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'e'); // |c|b|a|4|5|6|7| |e|d|
    MOS_TEST_CHECK(rb->pos.head, 8);
    MOS_TEST_CHECK(rb->pos.next_pos, 7);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'f'); // |c|b|a|4|5|6|7|f|e|d|
    MOS_TEST_CHECK(rb->pos.head, 7);
    MOS_TEST_CHECK(rb->pos.next_pos, 7);
    MOS_TEST_CHECK(written, 1);

    written = ring_buffer_push_front_byte(rb, 'g'); // |c|b|a|4|5|6|7|f|e|d|
    MOS_TEST_CHECK(rb->pos.head, 7);
    MOS_TEST_CHECK(rb->pos.next_pos, 7);
    MOS_TEST_CHECK(written, 0);

    full = ring_buffer_is_full(rb);
    MOS_TEST_CHECK(full, true);

    c = ring_buffer_pop_back_byte(rb); // |c|b|a|4|5|6| |f|e|d|
    MOS_TEST_CHECK(c, '7');
    MOS_TEST_CHECK(rb->pos.head, 7);
    MOS_TEST_CHECK(rb->pos.next_pos, 6);

    written = ring_buffer_push_front_byte(rb, 'h'); // |c|b|a|4|5|6|h|f|e|d|
    MOS_TEST_CHECK(written, 1);
    MOS_TEST_CHECK(rb->pos.head, 6);
    MOS_TEST_CHECK(rb->pos.next_pos, 6);

    c = ring_buffer_pop_front_byte(rb); // |c|b|a|4|5|6| |f|e|d|
    MOS_TEST_CHECK(c, 'h');
    MOS_TEST_CHECK(rb->pos.head, 7);
    MOS_TEST_CHECK(rb->pos.next_pos, 6);

    written = ring_buffer_push_back_byte(rb, 'i'); // |c|b|a|4|5|6|i|f|e|d|
    MOS_TEST_CHECK(written, 1);
    MOS_TEST_CHECK(rb->pos.head, 7);
    MOS_TEST_CHECK(rb->pos.next_pos, 7);
}

MOS_TEST_CASE(ringbuffer_push_pop_multiple_bytes)
{
    ring_buffer_t *rb = ring_buffer_create(20);
    const char *data = "MY_DATA!";

    size_t written;
    written = ring_buffer_push_back(rb, (u8 *) data, 8);
    MOS_TEST_CHECK(written, 8);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 8);
    MOS_TEST_CHECK(rb->pos.size, 8);

    char buf[8];
    size_t read;
    read = ring_buffer_pop_front(rb, (u8 *) buf, 8);
    MOS_TEST_CHECK(read, 8);
    MOS_TEST_CHECK(rb->pos.head, 8);
    MOS_TEST_CHECK(rb->pos.next_pos, 8);
    MOS_TEST_CHECK(rb->pos.size, 0);

    MOS_TEST_CHECK(strncmp(buf, data, 8), 0);
    MOS_TEST_CHECK(ring_buffer_is_empty(rb), true);
    rb->pos.head = 0;
    rb->pos.next_pos = 0;
    rb->pos.size = 0;

    written = ring_buffer_push_back(rb, (u8 *) data, 8);
    MOS_TEST_CHECK(written, 8);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 8);
    MOS_TEST_CHECK(rb->pos.size, 8);

    MOS_TEST_CHECK(strncmp((char *) rb->data, "MY_DATA!", 8), 0);

    written = ring_buffer_push_back(rb, (u8 *) data, 8);
    MOS_TEST_CHECK(written, 8);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 16);
    MOS_TEST_CHECK(rb->pos.size, 16);

    MOS_TEST_CHECK(strncmp((char *) rb->data, "MY_DATA!MY_DATA!", 16), 0);

    written = ring_buffer_push_back(rb, (u8 *) data, 8); // can't write more
    MOS_TEST_CHECK(written, 0);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 16);
    MOS_TEST_CHECK(rb->pos.size, 16);

    read = ring_buffer_pop_front(rb, (u8 *) buf, 8);
    MOS_TEST_CHECK(read, 8);
    MOS_TEST_CHECK(rb->pos.head, 8);
    MOS_TEST_CHECK(rb->pos.next_pos, 16);
    MOS_TEST_CHECK(rb->pos.size, 8);
    MOS_TEST_CHECK(strncmp(buf, data, 8), 0);

    MOS_TEST_CHECK(strncmp((char *) rb->data + 8, "MY_DATA!", 8), 0);

    written = ring_buffer_push_back(rb, (u8 *) data, 8); // wrap around
    MOS_TEST_CHECK(written, 8);
    MOS_TEST_CHECK(rb->pos.head, 8);
    MOS_TEST_CHECK(rb->pos.next_pos, 4);
    MOS_TEST_CHECK(rb->pos.size, 16);

    MOS_TEST_CHECK(strncmp((char *) rb->data, "ATA!ATA!MY_DATA!MY_D", 16), 0);

    read = ring_buffer_pop_front(rb, (u8 *) buf, 8);
    MOS_TEST_CHECK(read, 8);
    MOS_TEST_CHECK(rb->pos.head, 16);
    MOS_TEST_CHECK(rb->pos.next_pos, 4);
    MOS_TEST_CHECK(rb->pos.size, 8);
    MOS_TEST_CHECK(strncmp(buf, data, 8), 0);

    read = ring_buffer_pop_back(rb, (u8 *) buf, 8);
    MOS_TEST_CHECK(read, 8);
    MOS_TEST_CHECK(rb->pos.head, 16);
    MOS_TEST_CHECK(rb->pos.next_pos, 16);
    MOS_TEST_CHECK(rb->pos.size, 0);
    MOS_TEST_CHECK(strncmp(buf, data, 8), 0);

    written = ring_buffer_push_front(rb, (u8 *) data, 8); // no wrap around
    MOS_TEST_CHECK(written, 8);
    MOS_TEST_CHECK(rb->pos.head, 8);
    MOS_TEST_CHECK(rb->pos.next_pos, 16);
    MOS_TEST_CHECK(rb->pos.size, 8);

    written = ring_buffer_push_front(rb, (u8 *) data, 8);
    MOS_TEST_CHECK(written, 8);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 16);
    MOS_TEST_CHECK(rb->pos.size, 16);

    written = ring_buffer_push_front(rb, (u8 *) data, 8); // can't write more
    MOS_TEST_CHECK(written, 0);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 16);
    MOS_TEST_CHECK(rb->pos.size, 16);

    read = ring_buffer_pop_back(rb, (u8 *) buf, 8);
    MOS_TEST_CHECK(read, 8);
    MOS_TEST_CHECK(rb->pos.head, 0);
    MOS_TEST_CHECK(rb->pos.next_pos, 8);
    MOS_TEST_CHECK(rb->pos.size, 8);

    read = ring_buffer_push_front(rb, (u8 *) data, 8); // wrap around
    MOS_TEST_CHECK(read, 8);
    MOS_TEST_CHECK(rb->pos.head, 12);
    MOS_TEST_CHECK(rb->pos.next_pos, 8);
    MOS_TEST_CHECK(rb->pos.size, 16);

    read = ring_buffer_pop_back(rb, (u8 *) buf, 8);
    MOS_TEST_CHECK(read, 8);
    MOS_TEST_CHECK(rb->pos.head, 12);
    MOS_TEST_CHECK(rb->pos.next_pos, 0);
    MOS_TEST_CHECK(rb->pos.size, 8);
}
