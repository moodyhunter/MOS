// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/elf/elf.h"
#include "mos/filesystem/vfs.h"
#include "mos/mm/cow.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/signal.h"
#include "mos/tasks/task_types.h"
#include "mos/tasks/thread.h"

#include <mos/types.h>
#include <mos_stdlib.h>
#include <mos_string.h>

long process_do_execveat(process_t *process, fd_t dirfd, const char *path, const char *const argv[], const char *const envp[], int flags)
{
    thread_t *const thread = current_thread;
    process_t *const proc = current_process;

    MOS_ASSERT(thread->owner == process); // why

    MOS_UNUSED(flags); // not implemented: AT_EMPTY_PATH, AT_SYMLINK_NOFOLLOW
    file_t *f = vfs_openat(dirfd, path, OPEN_READ | OPEN_EXECUTE);
    if (IS_ERR(f))
        return PTR_ERR(f);

    io_ref(&f->io);
    elf_header_t header;
    if (!elf_read_and_verify_executable(f, &header))
    {
        pr_warn("failed to read elf header");
        io_unref(&f->io);
        return -ENOEXEC;
    }

    // !! ====== point of no return ====== !! //

    if (proc->name)
        kfree(proc->name);
    if (thread->name)
        kfree(thread->name);

    proc->name = strdup(f->dentry->name);   // set process name to the name of the executable
    thread->name = strdup(f->dentry->name); // set thread name to the name of the executable

    spinlock_acquire(&thread->state_lock);

    // backup argv and envp
    const char **argv_copy = NULL;
    const char **envp_copy = NULL;
    const char *path_copy = strdup(path);

    int argc = 0;
    while (argv && argv[argc])
        argc++, argv_copy = krealloc(argv_copy, (argc + 1) * sizeof(char *)), argv_copy[argc - 1] = strdup(argv[argc - 1]);
    argv_copy[argc] = NULL;
    int envc = 0;
    while (envp && envp[envc])
        envc++, envp_copy = krealloc(envp_copy, (envc + 1) * sizeof(char *)), envp_copy[envc - 1] = strdup(envp[envc - 1]);

    list_foreach(thread_t, t, process->threads)
    {
        if (t != thread)
        {
            signal_send_to_thread(t, SIGKILL); // nice
            thread_wait_for_tid(t->tid);
            thread_destroy(t);
        }
    }

    proc->main_thread = thread; // make current thread the only thread

    // free old memory
    spinlock_acquire(&proc->mm->mm_lock);
    list_foreach(vmap_t, vmap, proc->mm->mmaps)
    {
        spinlock_acquire(&vmap->lock);
        vmap_destroy(vmap); // no need to unlock because it's destroyed
    }
    spinlock_release(&proc->mm->mm_lock);

    // the userspace stack for the current thread will also be freed, so we create a new one
    if (thread->mode == THREAD_MODE_USER)
    {
        vmap_t *stack_vmap = cow_allocate_zeroed_pages(proc->mm, MOS_STACK_PAGES_USER, MOS_ADDR_USER_STACK, VALLOC_DEFAULT, VM_USER_RW);
        stack_init(&thread->u_stack, (void *) stack_vmap->vaddr, MOS_STACK_PAGES_USER * MOS_PAGE_SIZE);
        vmap_finalise_init(stack_vmap, VMAP_STACK, VMAP_TYPE_PRIVATE);
    }

    const bool filled = elf_fill_process(proc, f, path_copy, argv_copy, envp_copy);
    io_unref(&f->io);

    // free old argv and envp
    for (int i = 0; i < argc; i++)
        kfree(argv_copy[i]);
    kfree(argv_copy);
    for (int i = 0; i < envc; i++)
        kfree(envp_copy[i]);
    kfree(envp_copy);
    kfree(path_copy);

    if (!filled)
    {
        pr_emerg("failed to fill process, execve failed");
        spinlock_release(&thread->state_lock);
        process_handle_exit(proc, 0, SIGKILL);
        MOS_UNREACHABLE();
    }

    memzero(proc->signal_info.handlers, sizeof(proc->signal_info.handlers)); // reset signal handlers

    vmap_t *heap = cow_allocate_zeroed_pages(proc->mm, 1, MOS_ADDR_USER_HEAP, VALLOC_DEFAULT, VM_USER_RW);
    vmap_finalise_init(heap, VMAP_HEAP, VMAP_TYPE_PRIVATE);

    spinlock_release(&thread->state_lock);
    platform_return_to_userspace(platform_thread_regs(thread));
}
