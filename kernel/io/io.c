// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"
#include "mos/platform/platform.h"

#include <mos/io/io.h>
#include <mos/io/io_types.h>
#include <mos/mm/mm_types.h>
#include <mos/mos_global.h>
#include <mos/syslog/printk.h>
#include <mos_stdio.h>

static size_t _null_read(io_t *io, void *buffer, size_t size)
{
    (void) io;
    (void) buffer;
    (void) size;
    return 0;
}

static size_t _null_write(io_t *io, const void *buffer, size_t size)
{
    (void) io;
    (void) buffer;
    (void) size;
    return 0;
}

static io_t io_null_impl = {
    .refcount = 1, // never gets closed
    .type = IO_NULL,
    .flags = IO_READABLE | IO_WRITABLE,
    .ops =
        &(io_op_t){
            .read = _null_read,
            .write = _null_write,
            .seek = NULL,
            .close = NULL,
        },
};

io_t *const io_null = &io_null_impl;

void io_init(io_t *io, io_type_t type, io_flags_t flags, const io_op_t *ops)
{
    pr_dinfo2(io, "io_init(%p, %d, %d, %p)", (void *) io, type, flags, (void *) ops);

    if (unlikely(!io))
        mos_warn("io is NULL");

    if (unlikely(!ops))
        mos_warn("io->ops is NULL");

    if (flags & IO_READABLE)
        if (unlikely(!ops->read))
            mos_warn("ops->read is NULL for readable io");

    if (flags & IO_WRITABLE)
        if (unlikely(!ops->write))
            mos_warn("ops->write is NULL for writable io");

    if (flags & IO_SEEKABLE)
        if (unlikely(!ops->seek))
            mos_warn("io->ops->seek is NULL for seekable io");

    io->flags = flags;
    io->type = type;
    io->ops = ops;
    io->closed = false;
    io->refcount = 0;
}

io_t *io_ref(io_t *io)
{
    pr_dinfo2(io, "io_ref(%p)", (void *) io);
    if (unlikely(!io))
    {
        mos_warn("io is NULL");
        return NULL;
    }

    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return 0;
    }

    io->refcount++;
    return io;
}

io_t *io_unref(io_t *io)
{
    pr_dinfo2(io, "io_unref(%p)", (void *) io);
    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return NULL;
    }

    if (unlikely(io->refcount == 0))
    {
        mos_warn("%p has refcount 0", (void *) io);
        return NULL;
    }

    io->refcount--;

    if (io->refcount == 0)
    {
        if (io->ops->close)
        {
            pr_dinfo2(io, "closing %p", (void *) io);
            io->closed = true;
            io->ops->close(io);
        }
        else
        {
            pr_dinfo2(io, "%p is not closeable", (void *) io);
        }
        return NULL;
    }

    return io;
}

bool io_valid(const io_t *io)
{
    return io && !io->closed && io->refcount > 0 && io->ops;
}

size_t io_read(io_t *io, void *buf, size_t count)
{
    pr_dinfo2(io, "io_read(%p, %p, %zu)", (void *) io, buf, count);

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

    return io->ops->read(io, buf, count);
}

size_t io_pread(io_t *io, void *buf, size_t count, off_t offset)
{
    pr_dinfo2(io, "io_pread(%p, %p, %zu, %lu)", (void *) io, buf, count, offset);

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

    if (!(io->flags & IO_SEEKABLE))
    {
        pr_info2("%p is not seekable\n", (void *) io);
        return 0;
    }

    const off_t old_offset = io_tell(io);
    io_seek(io, offset, IO_SEEK_SET);
    const size_t ret = io_read(io, buf, count);
    io_seek(io, old_offset, IO_SEEK_SET);
    return ret;
}

size_t io_write(io_t *io, const void *buf, size_t count)
{
    pr_dinfo2(io, "io_write(%p, %p, %zu)", (void *) io, buf, count);

    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return 0;
    }

    if (!(io->flags & IO_WRITABLE))
    {
        pr_info2("%p is not writable", (void *) io);
        return 0;
    }

    return io->ops->write(io, buf, count);
}

off_t io_seek(io_t *io, off_t offset, io_seek_whence_t whence)
{
    pr_dinfo2(io, "io_seek(%p, %lu, %d)", (void *) io, offset, whence);

    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return 0;
    }

    if (!(io->flags & IO_SEEKABLE))
    {
        pr_info2("%p is not seekable", (void *) io);
        return 0;
    }

    return io->ops->seek(io, offset, whence);
}

off_t io_tell(io_t *io)
{
    pr_dinfo2(io, "io_tell(%p)", (void *) io);
    return io_seek(io, 0, IO_SEEK_CURRENT);
}

bool io_mmap_perm_check(io_t *io, vm_flags flags, bool is_private)
{
    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return false;
    }

    if (!(io->flags & IO_MMAPABLE))
    {
        pr_info2("%p is not mmapable", (void *) io);
        return false;
    }

    if (!(io->flags & IO_READABLE))
        return false; // can't mmap if io is not readable

    if (flags & VM_WRITE)
    {
        const bool may_mmap_writeable = is_private || io->flags & IO_WRITABLE;
        if (!may_mmap_writeable)
            return false; // can't mmap writable if io is not writable and not private
    }

    // if (flags & VM_EXEC && !(io->flags & IO_EXECUTABLE))
    // return false; // can't mmap executable if io is not executable

    return true;
}

bool io_mmap(io_t *io, vmap_t *vmap, off_t offset)
{
    pr_dinfo2(io, "io_mmap(%p, %p, %lu)", (void *) io, (void *) vmap, offset);
    if (!io_mmap_perm_check(io, vmap->vmflags, vmap->type == VMAP_TYPE_PRIVATE))
        return false;

    vmap->io = io;
    vmap->io_offset = offset;

    if (!io->ops->mmap(io, vmap, offset))
        return false;

    if (unlikely(!vmap->on_fault))
        mos_panic("vmap->on_fault is NULL, possibly buggy io->ops->mmap() implementation");

    io_ref(io); // mmap increases refcount
    return true;
}

bool io_munmap(io_t *io, vmap_t *vmap, bool *unmapped)
{
    pr_dinfo2(io, "io_unmap(%p, %p, %p)", (void *) io, (void *) vmap, (void *) unmapped);
    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return false;
    }

    if (unlikely(!vmap->io))
    {
        mos_warn("vmap->io is NULL");
        return false;
    }

    if (unlikely(vmap->io != io))
    {
        mos_warn("vmap->io != io");
        return false;
    }

    if (vmap->io->ops->munmap)
    {
        if (unlikely(!vmap->io->ops->munmap(vmap->io, vmap, unmapped)))
        {
            mos_warn("vmap->io->ops->unmap() failed");
            return false;
        }
    }

    io_unref(io); // unmap decreases refcount
    return true;
}

void io_get_name(const io_t *io, char *buf, size_t size)
{
    if (!io_valid(io))
    {
        snprintf(buf, size, "<invalid io %p>", (void *) io);
        return;
    }

    if (io->ops->get_name)
        io->ops->get_name(io, buf, size);
    else
        snprintf(buf, size, "<unnamed io %p>", (void *) io);
}
