// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/io/io.h>
#include <mos/mos_global.h>
#include <mos/printk.h>

void io_init(io_t *io, io_type_t type, io_flags_t flags, const io_op_t *ops)
{
    io->flags = flags;
    io->type = type;
    io->ops = ops;
    io->closed = false;
    io->refcount = 0;
}

io_t *io_ref(io_t *io)
{
    mos_debug(io, "io_ref(%p)", (void *) io);
    if (unlikely(!io))
    {
        mos_warn("io is NULL");
        return NULL;
    }

    if (unlikely(io->closed))
    {
        mos_warn("io_write: %p is already closed", (void *) io);
        return 0;
    }

    io->refcount++;
    return io;
}

void io_unref(io_t *io)
{
    mos_debug(io, "io_unref(%p)", (void *) io);
    if (unlikely(!io))
    {
        mos_warn("io is NULL");
        return;
    }

    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return;
    }

    if (--io->refcount > 0)
        return;

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
    if (unlikely(!io))
    {
        mos_warn("io is NULL");
        return 0;
    }
    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return 0;
    }
    if (!(io->flags & IO_READABLE))
    {
        pr_info2("%p is not readable\n", (void *) io);
        return 0;
    }
    if (unlikely(!io->ops->read))
    {
        mos_warn_once("no read function");
        return 0;
    }
    return io->ops->read(io, buf, count);
}

size_t io_write(io_t *io, const void *buf, size_t count)
{
    if (unlikely(!io))
    {
        mos_warn("io is NULL");
        return 0;
    }
    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return 0;
    }
    if (!(io->flags & IO_WRITABLE))
    {
        pr_info2("%p is not writable\n", (void *) io);
        return 0;
    }
    if (unlikely(!io->ops->write))
    {
        mos_warn_once("no write function");
        return 0;
    }
    return io->ops->write(io, buf, count);
}
