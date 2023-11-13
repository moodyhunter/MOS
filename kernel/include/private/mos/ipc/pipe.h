// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/io/io.h"
#include "mos/tasks/wait.h"

#include <mos/lib/structures/ring_buffer.h>

typedef struct
{
    u32 magic;
    waitlist_t waitlist; ///< for both reader and writer, only one party can wait on the pipe at a time
    spinlock_t lock;     ///< protects the buffer_pos (and thus the buffer)
    bool other_closed;   ///< true if the other end of the pipe has been closed
    void *buffers;
    size_t buffer_npages;
    ring_buffer_pos_t buffer_pos;
} pipe_t;

pipe_t *pipe_create(size_t bufsize);
size_t pipe_read(pipe_t *pipe, void *buf, size_t size);
size_t pipe_write(pipe_t *pipe, const void *buf, size_t size);

/**
 * @brief Close one end of the pipe, so that the other end will get EOF.
 * @note The other end should also call this function to get the pipe correctly freed.
 *
 * @param pipe The pipe to close one end of.
 * @return true if the pipe was fully closed, false if the other end is still open.
 */
__nodiscard bool pipe_close_one_end(pipe_t *pipe);

typedef struct
{
    io_t io_r, io_w;
    pipe_t *pipe;
} pipeio_t;

pipeio_t *pipeio_create(pipe_t *pipe);
