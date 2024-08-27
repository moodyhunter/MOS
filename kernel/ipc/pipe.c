// SPDX-License-Identifier: GPL-3.0-or-later
// abstract pipe implementation
// A pipe is a buffer that only has a single reader and a single writer

#include <limits.h>
#define pr_fmt(fmt) "pipe: " fmt

#include "mos/ipc/pipe.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform.h"
#include "mos/syslog/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/signal.h"
#include "mos/tasks/wait.h"

#include <mos/lib/sync/spinlock.h>
#include <mos_stdlib.h>

#define PIPE_MAGIC MOS_FOURCC('P', 'I', 'P', 'E')

static slab_t *pipe_slab = NULL;
SLAB_AUTOINIT("pipe", pipe_slab, pipe_t);

static slab_t *pipeio_slab = NULL;
SLAB_AUTOINIT("pipeio", pipeio_slab, pipeio_t);

#define advance_buffer(buffer, bytes) ((buffer) = (void *) ((char *) (buffer) + (bytes)))

size_t pipe_write(pipe_t *pipe, const void *buf, size_t size)
{
    if (pipe->magic != PIPE_MAGIC)
    {
        pr_warn("pipe_io_write: invalid magic");
        return 0;
    }

    pr_dinfo2(pipe, "writing %zu bytes", size);

    // write data to buffer
    spinlock_acquire(&pipe->lock);

    if (pipe->other_closed)
    {
        pr_dinfo2(pipe, "pipe closed");
        signal_send_to_thread(current_thread, SIGPIPE);
        spinlock_release(&pipe->lock);
        return -EPIPE; // pipe closed
    }

    size_t total_written = 0;

retry_write:;
    const size_t written = ring_buffer_pos_push_back(pipe->buffers, &pipe->buffer_pos, buf, size);
    advance_buffer(buf, written), size -= written, total_written += written;

    if (size > 0)
    {
        // buffer is full, wait for the reader to read some data
        pr_dinfo2(pipe, "pipe buffer full, waiting...");
        spinlock_release(&pipe->lock);
        waitlist_wake(&pipe->waitlist, INT_MAX);              // wake up any readers that are waiting for data
        MOS_ASSERT(reschedule_for_waitlist(&pipe->waitlist)); // wait for the reader to read some data
        if (signal_has_pending())
        {
            pr_dinfo2(pipe, "signal pending, returning early");
            return total_written;
        }
        spinlock_acquire(&pipe->lock);

        // check if the pipe is still valid
        if (pipe->other_closed)
        {
            pr_dinfo2(pipe, "pipe closed");
            signal_send_to_thread(current_thread, SIGPIPE);
            spinlock_release(&pipe->lock);
            return -EPIPE; // pipe closed
        }

        goto retry_write;
    }

    spinlock_release(&pipe->lock);

    // wake up any readers that are waiting for data
    waitlist_wake(&pipe->waitlist, INT_MAX);
    return total_written;
}

size_t pipe_read(pipe_t *pipe, void *buf, size_t size)
{
    if (pipe->magic != PIPE_MAGIC)
    {
        pr_warn("pipe_io_read: invalid magic");
        return 0;
    }

    pr_dinfo2(pipe, "reading %zu bytes", size);

    // read data from buffer
    spinlock_acquire(&pipe->lock);

    size_t total_read = 0;

retry_read:;
    const size_t read = ring_buffer_pos_pop_front(pipe->buffers, &pipe->buffer_pos, buf, size);
    advance_buffer(buf, read), size -= read, total_read += read;

    if (size > 0)
    {
        // check if the pipe is still valid
        if (pipe->other_closed && ring_buffer_pos_is_empty(&pipe->buffer_pos))
        {
            pr_dinfo2(pipe, "pipe closed");
            spinlock_release(&pipe->lock);
            waitlist_wake(&pipe->waitlist, INT_MAX);
            pr_dinfo2(pipe, "read %zu bytes", total_read);
            return total_read; // EOF
        }

        // buffer is empty, wait for the writer to write some data
        pr_dinfo2(pipe, "pipe buffer empty, waiting...");
        spinlock_release(&pipe->lock);
        waitlist_wake(&pipe->waitlist, INT_MAX);              // wake up any writers that are waiting for space in the buffer
        MOS_ASSERT(reschedule_for_waitlist(&pipe->waitlist)); // wait for the writer to write some data
        if (signal_has_pending())
        {
            pr_dinfo2(pipe, "signal pending, returning early");
            return total_read;
        }
        spinlock_acquire(&pipe->lock);
        goto retry_read;
    }

    spinlock_release(&pipe->lock);

    // wake up any writers that are waiting for space in the buffer
    waitlist_wake(&pipe->waitlist, INT_MAX);

    pr_dinfo2(pipe, "read %zu bytes", total_read);
    return total_read;
}

bool pipe_close_one_end(pipe_t *pipe)
{
    if (pipe->magic != PIPE_MAGIC)
    {
        pr_warn("pipe_io_close: invalid magic");
        return false;
    }

    spinlock_acquire(&pipe->lock);
    if (!pipe->other_closed)
    {
        pipe->other_closed = true;
        spinlock_release(&pipe->lock);

        // wake up any readers/writers that are waiting for data/space in the buffer
        waitlist_wake(&pipe->waitlist, INT_MAX);
        return false;
    }
    else
    {
        // the other end of the pipe is already closed, so we can just free the pipe
        spinlock_release(&pipe->lock);

        mm_free_pages(va_phyframe(pipe->buffers), pipe->buffer_pos.capacity / MOS_PAGE_SIZE);
        kfree(pipe);
        return true;
    }

    MOS_UNREACHABLE();
}

pipe_t *pipe_create(size_t bufsize)
{
    bufsize = ALIGN_UP_TO_PAGE(bufsize);

    pipe_t *pipe = kmalloc(pipe_slab);
    pipe->magic = PIPE_MAGIC;
    pipe->buffers = (void *) phyframe_va(mm_get_free_pages(bufsize / MOS_PAGE_SIZE));
    waitlist_init(&pipe->waitlist);
    ring_buffer_pos_init(&pipe->buffer_pos, bufsize);
    return pipe;
}

static size_t pipeio_io_read(io_t *io, void *buf, size_t size)
{
    MOS_ASSERT(io->flags & IO_READABLE);
    pipeio_t *pipeio = container_of(io, pipeio_t, io_r);
    return pipe_read(pipeio->pipe, buf, size);
}

static size_t pipeio_io_write(io_t *io, const void *buf, size_t size)
{
    MOS_ASSERT(io->flags & IO_WRITABLE);
    pipeio_t *pipeio = container_of(io, pipeio_t, io_w);
    return pipe_write(pipeio->pipe, buf, size);
}

static void pipeio_io_close(io_t *io)
{
    const char *type = "<unknown>";
    const pipeio_t *const pipeio = statement_expr(const pipeio_t *, {
        if (io->flags & IO_READABLE)
            retval = container_of(io, pipeio_t, io_r), type = "reader"; // the reader is closing
        else if (io->flags & IO_WRITABLE)
            retval = container_of(io, pipeio_t, io_w), type = "writer"; // the writer is closing
        else
            MOS_UNREACHABLE();
    });

    if (!pipeio->pipe->other_closed)
        pr_dinfo2(pipe, "pipe %s closing", type);
    else
        pr_dinfo2(pipe, "pipe is already closed by the other end, '%s' closing", type);

    const bool fully_closed = pipe_close_one_end(pipeio->pipe);
    if (fully_closed)
        kfree(pipeio);
}

static const io_op_t pipe_io_ops = {
    .write = pipeio_io_write,
    .read = pipeio_io_read,
    .close = pipeio_io_close,
};

pipeio_t *pipeio_create(pipe_t *pipe)
{
    pipeio_t *pipeio = kmalloc(pipeio_slab);
    pipeio->pipe = pipe;
    io_init(&pipeio->io_r, IO_PIPE, IO_READABLE, &pipe_io_ops);
    io_init(&pipeio->io_w, IO_PIPE, IO_WRITABLE, &pipe_io_ops);
    return pipeio;
}
