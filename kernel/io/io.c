// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/io/io.h"

#include "mos/printk.h"

void io_ref(io_t *io)
{
    mos_debug("io_ref(%p)", (void *) io);
    io->refcount.atomic++;
}

void io_unref(io_t *io)
{
    mos_debug("io_unref(%p)", (void *) io);
    io->refcount.atomic--;
    if (io->refcount.atomic > 0)
        return;

    io_close(io);
}

size_t io_read(io_t *io, void *buf, size_t count)
{
    if (unlikely(!io->ops->read))
    {
        mos_warn_once("io_read: no read function");
        return 0;
    }
    return io->ops->read(io, buf, count);
}
size_t io_write(io_t *io, const void *buf, size_t count)
{
    if (unlikely(!io->ops->write))
    {
        mos_warn("io_write: no write function");
        return 0;
    }
    return io->ops->write(io, buf, count);
}

void io_close(io_t *io)
{
    if (unlikely(!io->ops->close))
    {
        mos_warn("io_close: no close function");
        return;
    }
    io->ops->close(io);
}
