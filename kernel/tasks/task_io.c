// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/task_io.h"

#include "lib/containers.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

static size_t stdin_read(io_t *io, void *buf, size_t count)
{
    stdio_t *stdio = container_of(io, stdio_t, io);
    MOS_ASSERT(stdio->type == STDIO_TYPE_STDIN);
    MOS_UNUSED(io);
    MOS_UNUSED(buf);
    MOS_UNUSED(count);
    return 0;
}

static size_t stdout_write(io_t *io, const void *buf, size_t count)
{
    if (!buf)
        return 0;
    stdio_t *stdio = container_of(io, stdio_t, io);
    MOS_ASSERT(stdio->type == STDIO_TYPE_STDOUT);
    pr_warn("%.*s", (int) count, (const char *) buf);
    return 0;
}

static size_t stderr_write(io_t *io, const void *buf, size_t count)
{
    if (!buf)
        return 0;
    stdio_t *stdio = container_of(io, stdio_t, io);
    MOS_ASSERT(stdio->type == STDIO_TYPE_STDERR);
    pr_warn("stderr: %.*s", (int) count, (const char *) buf);
    return 0;
}

static io_op_t task_stdin_op = { .read = stdin_read };
static io_op_t task_stdout_op = { .write = stdout_write };
static io_op_t task_stderr_op = { .write = stderr_write };

void process_stdio_setup(process_t *process)
{
    stdio_t *stdin = kcalloc(1, sizeof(stdio_t));
    stdio_t *stdout = kcalloc(1, sizeof(stdio_t));
    stdio_t *stderr = kcalloc(1, sizeof(stdio_t));

    stdin->type = STDIO_TYPE_STDIN;
    stdout->type = STDIO_TYPE_STDOUT;
    stderr->type = STDIO_TYPE_STDERR;

    io_init(&stdin->io, IO_READABLE, -1, &task_stdin_op);
    io_init(&stdout->io, IO_WRITABLE, -1, &task_stdout_op);
    io_init(&stderr->io, IO_WRITABLE, -1, &task_stderr_op);

    process->file_table[0] = &stdin->io;
    process->file_table[1] = &stdout->io;
    process->file_table[2] = &stderr->io;
    process->files_count = 3;
}
