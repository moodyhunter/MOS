// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.hpp"

#include <mos/io/io.hpp>
#include <mos/io/io_types.h>
#include <mos/mm/mm_types.h>
#include <mos/mos_global.h>
#include <mos/syslog/printk.hpp>
#include <mos_stdio.hpp>

struct NullIO final : IO
{
    NullIO() : IO(IO_READABLE | IO_WRITABLE, IO_NULL) {};
    virtual ~NullIO() {};
    void on_closed() override
    {
        mos_panic("null io cannot be closed");
    }

    size_t on_read(void *, size_t) override
    {
        return 0;
    }

    size_t on_write(const void *, size_t) override
    {
        return 0;
    }
};

static NullIO io_null_impl;
IO *const io_null = &io_null_impl;

IO::IO(IOFlags flags, io_type_t type) : io_flags(flags), io_type(type)
{
}

IO::~IO()
{
    if (!io_closed)
        mEmerg << "IO::~IO: " << (void *) (this) << " is not closed at destruction";
}

mos::string IO::name() const
{
    return "<unnamed io " + mos::to_string(this) + ">";
}

size_t IO::read(void *buf, size_t count)
{
    pr_dinfo2(io, "io_read(%p, %p, %zu)", (void *) this, buf, count);

    if (unlikely(io_closed))
    {
        mos_warn("%p is already closed", (void *) this);
        return 0;
    }

    if (!io_flags.test(IO_READABLE))
    {
        pr_info2("%p is not readable\n", (void *) this);
        return 0;
    }

    return on_read(buf, count);
}

size_t IO::pread(void *buf, size_t count, off_t offset)
{
    pr_dinfo2(io, "io_pread(%p, %p, %zu, %lu)", (void *) this, buf, count, offset);

    if (unlikely(io_closed))
    {
        mos_warn("%p is already closed", (void *) this);
        return 0;
    }

    if (!(io_flags.test(IO_READABLE)))
    {
        pr_info2("%p is not readable\n", (void *) this);
        return 0;
    }

    if (!(io_flags.test(IO_SEEKABLE)))
    {
        pr_info2("%p is not seekable\n", (void *) this);
        return 0;
    }

    const off_t old_offset = this->tell();
    this->seek(offset, IO_SEEK_SET);
    const size_t ret = read(buf, count);
    this->seek(old_offset, IO_SEEK_SET);
    return ret;
}

size_t IO::write(const void *buf, size_t count)
{
    pr_dinfo2(io, "io_write(%p, %p, %zu)", (void *) this, buf, count);

    if (unlikely(io_closed))
    {
        mos_warn("%p is already closed", (void *) this);
        return 0;
    }

    if (!(io_flags.test(IO_WRITABLE)))
    {
        pr_info2("%p is not writable", (void *) this);
        return 0;
    }

    return on_write(buf, count);
}

off_t IO::seek(off_t offset, io_seek_whence_t whence)
{
    pr_dinfo2(io, "io_seek(%p, %lu, %d)", (void *) this, offset, whence);

    if (unlikely(io_closed))
    {
        mos_warn("%p is already closed", (void *) this);
        return 0;
    }

    if (!io_flags.test(IO_SEEKABLE))
    {
        pr_info2("%p is not seekable", (void *) this);
        return 0;
    }

    return on_seek(offset, whence);
}

off_t IO::tell()
{
    dInfo2<io> << fmt("io_tell({})", (void *) this);
    return seek(0, IO_SEEK_CURRENT);
}

bool IO::VerifyMMapPermissions(VMFlags flags, bool is_private)
{
    if (unlikely(io_closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return false;
    }

    if (!(io_flags.test(IO_MMAPABLE)))
    {
        pr_info2("%p is not mmapable", (void *) io);
        return false;
    }

    if (!(io_flags.test(IO_READABLE)))
        return false; // can't mmap if io is not readable

    if (flags & VM_WRITE)
    {
        const bool may_mmap_writeable = is_private || io_flags.test(IO_WRITABLE);
        if (!may_mmap_writeable)
            return false; // can't mmap writable if io is not writable and not private
    }

    // if (flags & VM_EXEC && !(io->flags & IO_EXECUTABLE))
    // return false; // can't mmap executable if io is not executable

    return true;
}

bool IO::map(vmap_t *vmap, off_t offset)
{
    pr_dinfo2(io, "io_mmap(%p, %p, %lu)", (void *) this, (void *) vmap, offset);
    if (!VerifyMMapPermissions(vmap->vmflags, vmap->type == VMAP_TYPE_PRIVATE))
        return false;

    vmap->io = this;
    vmap->io_offset = offset;

    if (!this->on_mmap(vmap, offset))
        return false;

    if (unlikely(!vmap->on_fault))
        mos_panic("vmap->on_fault is NULL, possibly buggy io->ops->mmap() implementation");

    this->ref(); // mmap increases refcount
    return true;
}

bool IO::unmap(vmap_t *vmap, bool *unmapped)
{
    pr_dinfo2(io, "io_unmap(%p, %p, %p)", (void *) this, (void *) vmap, (void *) unmapped);
    if (unlikely(io_closed))
    {
        mos_warn("%p is already closed", (void *) this);
        return false;
    }

    if (unlikely(!vmap->io))
    {
        mos_warn("vmap->io is NULL");
        return false;
    }

    if (unlikely(vmap->io != this))
    {
        mos_warn("vmap->io != io");
        return false;
    }

    if (unlikely(!on_munmap(vmap, unmapped)))
    {
        mos_warn("vmap->io->ops->unmap() failed");
        return false;
    }

    this->unref(); // unmap decreases refcount
    return true;
}

size_t IO::on_read(void *, size_t)
{
    MOS_UNREACHABLE_X("IO %p is readable but does not implement on_read", (void *) this);
    return -ENOTSUP;
}

size_t IO::on_write(const void *, size_t)
{
    MOS_UNREACHABLE_X("IO %p is writable but does not implement on_write", (void *) this);
    return -ENOTSUP;
}

bool IO::on_mmap(vmap_t *, off_t)
{
    MOS_UNREACHABLE_X("IO %p is mappable but does not implement on_mmap", (void *) this);
    return false;
}

bool IO::on_munmap(vmap_t *, bool *)
{
    return false;
}

off_t IO::on_seek(off_t, io_seek_whence_t)
{
    MOS_UNREACHABLE_X("IO %p is seekable but does not implement on_seek", (void *) this);
}
