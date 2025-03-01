// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/assert.hpp"
#include "mos/mm/mm_types.hpp"
#include "mos/syslog/syslog.hpp"

#include <mos/io/io_types.h>
#include <mos/mm/mm_types.h>
#include <mos/types.hpp>

struct IO;
struct vmap_t; // forward declaration

typedef enum
{
    IO_NULL,    // null io port
    IO_FILE,    // a file
    IO_DIR,     // a directory (i.e. readdir())
    IO_IPC,     // an IPC channel
    IO_PIPE,    // an end of a pipe
    IO_CONSOLE, // a console
} io_type_t;

typedef enum
{
    IO_NONE = MEM_PERM_NONE,       // 0
    IO_READABLE = MEM_PERM_READ,   // 1 << 0
    IO_WRITABLE = MEM_PERM_WRITE,  // 1 << 1
    IO_EXECUTABLE = MEM_PERM_EXEC, // 1 << 2
    IO_SEEKABLE = 1 << 3,
    IO_MMAPABLE = 1 << 4,
} io_flags_t;
MOS_ENUM_FLAGS(io_flags_t, IOFlags);

struct IO
{
    const IOFlags io_flags = IO_NONE;
    const io_type_t io_type = IO_NULL;

    explicit IO(IOFlags flags, io_type_t type);
    virtual ~IO() = 0;

    static bool IsValid(const IO *io)
    {
        return io && !io->io_closed && io->io_refcount > 0;
    }

    friend mos::SyslogStreamWriter operator<<(mos::SyslogStreamWriter stream, const IO *io)
    {
        stream << fmt("\\{ '{}', {}}", io->name(), io->io_closed ? "closed" : "active");
        return stream;
    }

    inline IO *ref()
    {
        if (unlikely(io_closed))
        {
            mos_warn("%p is already closed", (void *) this);
            return 0;
        }

        io_refcount++;
        return this;
    }

    inline IO *unref()
    {
        if (unlikely(io_closed))
        {
            mos_warn("%p is already closed", (void *) this);
            return nullptr;
        }

        if (--io_refcount == 0)
        {
            io_closed = true;
            on_closed();
            return nullptr;
        }

        return this;
    }

    virtual mos::string name() const;
    virtual off_t seek(off_t, io_seek_whence_t) final;
    virtual off_t tell() final;

    virtual size_t read(void *buf, size_t count) final;
    virtual size_t pread(void *buf, size_t count, off_t offset) final;
    virtual size_t write(const void *buf, size_t count) final;

    virtual bool VerifyMMapPermissions(VMFlags flags, bool is_private) final;

    bool map(vmap_t *vmap, off_t offset);
    bool unmap(vmap_t *vmap, bool *unmapped);

  private:
    virtual void on_closed() = 0;
    virtual size_t on_read(void *, size_t);
    virtual size_t on_write(const void *, size_t);
    virtual bool on_mmap(vmap_t *, off_t);
    virtual bool on_munmap(vmap_t *, bool *);
    virtual off_t on_seek(off_t, io_seek_whence_t);

  private:
    bool io_closed = false;
    atomic_t io_refcount = 0;
};

extern IO *const io_null;
