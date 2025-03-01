// SPDX-License-Identifier: GPL-3.0-or-later
// abstract pipe implementation
// A pipe is a buffer that only has a single reader and a single writer

#include "mos/ipc/pipe.hpp"

#include "mos/platform/platform.hpp"
#include "mos/tasks/schedule.hpp"
#include "mos/tasks/signal.hpp"
#include "mos/tasks/wait.hpp"

#include <climits>
#include <mos/lib/sync/spinlock.hpp>
#include <mos_stdlib.hpp>

#define PIPE_MAGIC MOS_FOURCC('P', 'I', 'P', 'E')

#define advance_buffer(buffer, bytes) ((buffer) = (void *) ((char *) (buffer) + (bytes)))

size_t pipe_write(pipe_t *p, const void *buf, size_t size)
{
    if (p->magic != PIPE_MAGIC)
    {
        mWarn << "pipe_io_write: invalid magic";
        return 0;
    }

    dInfo2<pipe> << "writing " << size << " bytes";

    // write data to buffer
    spinlock_acquire(&p->lock);

    if (p->other_closed)
    {
        dInfo2<pipe> << "pipe closed";
        signal_send_to_thread(current_thread, SIGPIPE);
        spinlock_release(&p->lock);
        return -EPIPE; // pipe closed
    }

    size_t total_written = 0;

retry_write:;
    const size_t written = ring_buffer_pos_push_back((u8 *) p->buffers, &p->buffer_pos, (u8 *) buf, size);
    advance_buffer(buf, written), size -= written, total_written += written;

    if (size > 0)
    {
        // buffer is full, wait for the reader to read some data
        dInfo2<pipe> << "pipe buffer full, waiting...";
        spinlock_release(&p->lock);
        waitlist_wake(&p->waitlist, INT_MAX);              // wake up any readers that are waiting for data
        MOS_ASSERT(reschedule_for_waitlist(&p->waitlist)); // wait for the reader to read some data
        if (signal_has_pending())
        {
            dInfo2<pipe> << "signal pending, returning early";
            return total_written;
        }
        spinlock_acquire(&p->lock);

        // check if the pipe is still valid
        if (p->other_closed)
        {
            dInfo2<pipe> << "pipe closed";
            signal_send_to_thread(current_thread, SIGPIPE);
            spinlock_release(&p->lock);
            return -EPIPE; // pipe closed
        }

        goto retry_write;
    }

    spinlock_release(&p->lock);

    // wake up any readers that are waiting for data
    waitlist_wake(&p->waitlist, INT_MAX);
    return total_written;
}

size_t pipe_read(pipe_t *p, void *buf, size_t size)
{
    if (p->magic != PIPE_MAGIC)
    {
        mWarn << "pipe_io_read: invalid magic";
        return 0;
    }

    dInfo2<pipe> << "reading " << size << " bytes";

    // read data from buffer
    spinlock_acquire(&p->lock);

    size_t total_read = 0;

retry_read:;
    const size_t read = ring_buffer_pos_pop_front((u8 *) p->buffers, &p->buffer_pos, (u8 *) buf, size);
    advance_buffer(buf, read), size -= read, total_read += read;

    if (size > 0)
    {
        // check if the pipe is still valid
        if (p->other_closed && ring_buffer_pos_is_empty(&p->buffer_pos))
        {
            dInfo2<pipe> << "pipe closed";
            spinlock_release(&p->lock);
            waitlist_wake(&p->waitlist, INT_MAX);
            dInfo2<pipe> << "read " << total_read << " bytes";
            return total_read; // EOF
        }

        // buffer is empty, wait for the writer to write some data
        dInfo2<pipe> << "pipe buffer empty, waiting...";
        spinlock_release(&p->lock);
        waitlist_wake(&p->waitlist, INT_MAX);              // wake up any writers that are waiting for space in the buffer
        MOS_ASSERT(reschedule_for_waitlist(&p->waitlist)); // wait for the writer to write some data
        if (signal_has_pending())
        {
            dInfo2<pipe> << "signal pending, returning early";
            return total_read;
        }
        spinlock_acquire(&p->lock);
        goto retry_read;
    }

    spinlock_release(&p->lock);

    // wake up any writers that are waiting for space in the buffer
    waitlist_wake(&p->waitlist, INT_MAX);

    dInfo2<pipe> << "read " << total_read << " bytes";
    return total_read;
}

bool pipe_close_one_end(pipe_t *pipe)
{
    if (pipe->magic != PIPE_MAGIC)
    {
        mWarn << "pipe_io_close: invalid magic";
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
        delete pipe;
        return true;
    }

    MOS_UNREACHABLE();
}

PtrResult<pipe_t> pipe_create(size_t bufsize)
{
    bufsize = ALIGN_UP_TO_PAGE(bufsize);

    pipe_t *pipe = mos::create<pipe_t>();
    pipe->magic = PIPE_MAGIC;
    pipe->buffers = (void *) phyframe_va(mm_get_free_pages(bufsize / MOS_PAGE_SIZE));
    waitlist_init(&pipe->waitlist);
    ring_buffer_pos_init(&pipe->buffer_pos, bufsize);
    return pipe;
}

size_t PipeIOImpl::on_read(void *buf, size_t size)
{
    MOS_ASSERT(io_flags.test(IO_READABLE));
    pipeio_t *pipeio = container_of(this, pipeio_t, io_r);
    return pipe_read(pipeio->pipe, buf, size);
}

size_t PipeIOImpl::on_write(const void *buf, size_t size)
{
    MOS_ASSERT(io_flags.test(IO_WRITABLE));
    pipeio_t *pipeio = container_of(this, pipeio_t, io_w);
    return pipe_write(pipeio->pipe, buf, size);
}

void PipeIOImpl::on_closed()
{
    const char *type = "<unknown>";
    const pipeio_t *const pipeio = statement_expr(const pipeio_t *, {
        if (io_flags.test(IO_READABLE))
            retval = container_of(this, pipeio_t, io_r), type = "reader"; // the reader is closing
        else if (io_flags.test(IO_WRITABLE))
            retval = container_of(this, pipeio_t, io_w), type = "writer"; // the writer is closing
        else
            MOS_UNREACHABLE();
    });

    if (!pipeio->pipe->other_closed)
        dInfo2<pipe> << "pipe " << type << " closing";
    else
        dInfo2<pipe> << "pipe is already closed by the other end, '" << type << "' closing";

    const bool fully_closed = pipe_close_one_end(pipeio->pipe);
    if (fully_closed)
        delete pipeio;
}

pipeio_t *pipeio_create(pipe_t *pipe)
{
    return mos::create<pipeio_t>(pipe);
}
