// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/io/terminal.h"

#include "mos/device/console.h"
#include "mos/io/io.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

static size_t terminal_io_read(io_t *io, void *buffer, size_t size)
{
    terminal_t *terminal = container_of(io, terminal_t, io);
    switch (terminal->type)
    {
        case TERM_TYPE_CONSOLE: return console_read(terminal->console, buffer, size);
        case TERM_TYPE_PIPE: return io_read(terminal->pipe.read, buffer, size);
        default: MOS_UNREACHABLE();
    }
}

static size_t terminal_io_write(io_t *io, const void *buffer, size_t size)
{
    terminal_t *terminal = container_of(io, terminal_t, io);
    switch (terminal->type)
    {
        case TERM_TYPE_CONSOLE: return console_write(terminal->console, buffer, size);
        case TERM_TYPE_PIPE: return io_write(terminal->pipe.write, buffer, size);
        default: MOS_UNREACHABLE();
    }
}

static void terminal_io_close(io_t *io)
{
    terminal_t *terminal = container_of(io, terminal_t, io);
    switch (terminal->type)
    {
        case TERM_TYPE_CONSOLE: break;
        case TERM_TYPE_PIPE:
        {
            io_unref(terminal->pipe.read);
            io_unref(terminal->pipe.write);
        }
        break;
    }
    kfree(terminal);
}

static io_op_t terminal_io_op = {
    .read = terminal_io_read,
    .write = terminal_io_write,
    .close = terminal_io_close,
};

terminal_t *terminal_create_console(console_t *console)
{
    if (console == NULL)
    {
        mos_warn("console is NULL");
        return NULL;
    }
    terminal_t *terminal = kzalloc(sizeof(terminal_t));
    terminal->type = TERM_TYPE_CONSOLE;
    terminal->console = console;
    io_init(&terminal->io, IO_READABLE | IO_WRITABLE, &terminal_io_op);
    return terminal;
}

terminal_t *terminal_create_pipe(io_t *read, io_t *write)
{
    terminal_t *terminal = kzalloc(sizeof(terminal_t));
    terminal->type = TERM_TYPE_PIPE;
    terminal->pipe.read = io_ref(read);
    terminal->pipe.write = io_ref(write);
    io_init(&terminal->io, IO_READABLE | IO_WRITABLE, &terminal_io_op);
    return terminal;
}
