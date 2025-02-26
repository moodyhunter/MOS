// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/io/io.hpp"
#include "mos/tasks/wait.hpp"

#include <mos/allocator.hpp>
#include <mos/lib/structures/ring_buffer.hpp>

struct pipe_t : mos::NamedType<"Pipe">
{
    u32 magic;
    waitlist_t waitlist; ///< for both reader and writer, only one party can wait on the pipe at a time
    spinlock_t lock;     ///< protects the buffer_pos (and thus the buffer)
    bool other_closed;   ///< true if the other end of the pipe has been closed
    void *buffers;
    ring_buffer_pos_t buffer_pos;
};

PtrResult<pipe_t> pipe_create(size_t bufsize);
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

struct PipeIOImpl : IO
{
    explicit PipeIOImpl(IOFlags flags) : IO(flags, IO_PIPE) {};

    size_t on_read(void *buf, size_t size) override;
    size_t on_write(const void *buf, size_t size) override;
    void on_closed() override;
};

struct pipeio_t : mos::NamedType<"PipeIO">
{
    explicit pipeio_t(pipe_t *pipe) : pipe(pipe), io_r(IO_READABLE), io_w(IO_WRITABLE) {};

    pipe_t *const pipe;
    PipeIOImpl io_r, io_w;
};

pipeio_t *pipeio_create(pipe_t *pipe);
