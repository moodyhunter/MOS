// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/io/io.h"

#include "mos/mos_global.h"
#include "mos/printk.h"

void io_init(io_t *io, io_flags_t flags, size_t size, const io_op_t *ops)
{
    io->flags = flags;
    io->size = size;
    io->ops = ops;
    io->closed = false;
    refcount_zero(&io->refcount);
}

io_t *io_ref(io_t *io)
{
    mos_debug("io_ref(%p)", (void *) io);

    if (unlikely(io->closed))
    {
        mos_warn("io_write: %p is already closed", (void *) io);
        return 0;
    }

    refcount_inc(&io->refcount);
    return io;
}

void io_unref(io_t *io)
{
    mos_debug("io_unref(%p)", (void *) io);

    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return;
    }

    if (refcount_get(&io->refcount) == 0)
        return;

    refcount_dec(&io->refcount);

    if (unlikely(!io->ops->close))
    {
        mos_warn("no close function");
        return;
    }

    io->closed = true;
    io->ops->close(io);
}

size_t io_read(io_t *io, void *buf, size_t count)
{
    if (unlikely(io->closed))
    {
        mos_warn("io_write: %p is already closed", (void *) io);
        return 0;
    }
    if (!(io->flags & IO_READABLE))
    {
        pr_info2("io_read: %p is not readable\n", (void *) io);
        return 0;
    }
    if (unlikely(!io->ops->read))
    {
        mos_warn_once("io_read: no read function");
        return 0;
    }
    return io->ops->read(io, buf, count);
}

size_t io_write(io_t *io, const void *buf, size_t count)
{
    if (unlikely(io->closed))
    {
        mos_warn("io_write: %p is already closed", (void *) io);
        return 0;
    }
    if (!(io->flags & IO_WRITABLE))
    {
        pr_info2("io_write: %p is not writable\n", (void *) io);
        return 0;
    }
    if (unlikely(!io->ops->write))
    {
        mos_warn("io_write: no write function");
        return 0;
    }
    return io->ops->write(io, buf, count);
}
