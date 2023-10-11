// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/vfs.h"
#include "mos/platform/platform.h"
#include "mos/tasks/signal.h"
#include "mos/tasks/task_types.h"

#include <mos/types.h>

long process_do_execveat(process_t *process, fd_t dirfd, const char *path, const char *const argv[], const char *const envp[], int flags)
{
    thread_t *thread = current_thread;
    MOS_ASSERT(thread->owner == process); // why

    MOS_UNUSED(flags); // not implemented: AT_EMPTY_PATH, AT_SYMLINK_NOFOLLOW
    file_t *f = vfs_openat(dirfd, path, OPEN_READ | OPEN_EXECUTE);
    if (IS_ERR_OR_NULL(f))
    {
        pr_warn("failed to open file %s", path);
        return PTR_ERR(f);
    }

    list_foreach(thread_t, t, process->threads)
    {
        if (t != thread)
            signal_send_to_thread(thread, SIGKILL); // nice
    }

    return -ENOSYS;
}
