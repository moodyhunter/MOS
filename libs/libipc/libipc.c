// SPDX-License-Identifier: GPL-3.0-or-later

#include "libipc/ipc.h"

#include <liballoc.h>
#include <string.h>

#ifdef __MOS_KERNEL__
#include <mos/syscall/decl.h>
#define syscall_io_read(fd, buffer, size)  impl_syscall_io_read(fd, buffer, size)
#define syscall_io_write(fd, buffer, size) impl_syscall_io_write(fd, buffer, size)
#else
#include <mos/syscall/usermode.h>
#endif

ipc_msg_t *ipc_msg_create(size_t size)
{
    ipc_msg_t *buffer = liballoc_malloc(sizeof(ipc_msg_t) + size);
    memzero(buffer, sizeof(ipc_msg_t) + size);
    buffer->size = size;
    return buffer;
}

void ipc_msg_destroy(ipc_msg_t *buffer)
{
    liballoc_free(buffer);
}

ipc_msg_t *ipc_read_msg(fd_t fd)
{
    size_t size = 0;
    size_t read_size = syscall_io_read(fd, &size, sizeof(size));
    if (read_size != sizeof(size))
    {
        mos_warn("failed to read size from ipc channel");
        return NULL;
    }

    ipc_msg_t *buffer = ipc_msg_create(size);
    read_size = syscall_io_read(fd, buffer->data, buffer->size);
    if (read_size != size)
    {
        mos_warn("failed to read data from ipc channel");
        ipc_msg_destroy(buffer);
        return NULL;
    }

    return buffer;
}

bool ipc_write_msg(fd_t fd, ipc_msg_t *buffer)
{
    size_t written = syscall_io_write(fd, &buffer->size, sizeof(buffer->size));
    if (written != sizeof(buffer->size))
    {
        mos_warn("failed to write size to ipc channel");
        return false;
    }

    written = syscall_io_write(fd, buffer->data, buffer->size);
    if (written != buffer->size)
    {
        mos_warn("failed to write data to ipc channel");
        return false;
    }

    return true;
}

bool ipc_write_as_msg(fd_t fd, const char *data, size_t size)
{
    size_t w = 0;
    w = syscall_io_write(fd, &size, sizeof(size));
    if (unlikely(w != sizeof(size)))
    {
        mos_warn("failed to write size to ipc channel");
        return false;
    }
    w = syscall_io_write(fd, data, size);
    if (unlikely(w != size))
    {
        mos_warn("failed to write data to ipc channel");
        return false;
    }

    return true;
}

size_t ipc_read_as_msg(fd_t fd, char *buffer, size_t buffer_size)
{
    size_t r = 0;
    size_t data_size = 0;
    r = syscall_io_read(fd, &data_size, sizeof(size_t));
    if (unlikely(r != sizeof(size_t)))
    {
        mos_warn("failed to read size from ipc channel");
        return 0;
    }

    if (unlikely(data_size > buffer_size))
    {
        mos_warn("buffer too small");
        return 0;
    }

    r = syscall_io_read(fd, buffer, buffer_size);
    if (unlikely(r != data_size))
    {
        mos_warn("failed to read data from ipc channel");
        return 0;
    }
    return data_size;
}
