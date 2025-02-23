// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/assert.hpp"
#include "mos/misc/kallsyms.hpp"
#include "mos/platform/platform.hpp"
#include "mos/tasks/process.hpp"
#include "mos/tasks/task_types.hpp"
#include "mos/tasks/thread.hpp"

#include <mos/types.hpp>
#include <mos_stdio.hpp>

static size_t do_print_vmflags(char *buf, size_t size, vm_flags flags)
{
    return snprintf(buf, size, "%c%c%c", (flags & VM_READ) ? 'r' : '-', (flags & VM_WRITE) ? 'w' : '-', (flags & VM_EXEC) ? 'x' : '-');
}

/**
 * @brief Kernel's extension to vsnprintf, '%p' format specifier.
 *
 * @details Supported extensions are listed below:
 *
 *  '%ps'   prints a kernel symbol name, with offset if applicable.
 *              e.g. "do_fork (+0x123)"
 *  '%pt'   prints a thread_t object.
 *              e.g.: "[p123:my_process]"
 *  '%pp'   prints a process_t object.
 *              e.g.: "[t123:my_thread]"
 *  '%pvf'  prints vm_flags_t flag, only the r/w/x bits are printed for general purpose.
 *              e.g.: "rwx" / "r--" / "rw-" / "--x"
 *  '%pvm'  prints a vmap_t object.
 *              e.g.: "{ 0x123000-0x123fff, rwx, on_fault=0x12345678 }"
 *  '%pio'  prints an io_t object.
 *              e.g.: "{ 'file.txt', offset=0x12345678 }"
 *
 * @returns true if the format specifier is handled, false otherwise, the
 *          caller should fallback to the default implementation. (i.e. %p)
 *
 */

size_t vsnprintf_do_pointer_kernel(char *buf, size_t *size, const char **pformat, ptr_t ptr)
{
    size_t ret = 0;
#define current      (**pformat)
#define peek_next    (*(*pformat + 1))
#define shift_next   ((void) ((*pformat)++))
#define unshift_next ((void) ((*pformat)--))

#define wrap_print(...) wrap_printed(snprintf(buf, *size, __VA_ARGS__))
#define wrap_printed(x)                                                                                                                                                  \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        const size_t _printed = x;                                                                                                                                       \
        buf += _printed, ret += _printed;                                                                                                                                \
    } while (0)

    if (current != 'p')
        goto done;

#define null_check()                                                                                                                                                     \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (unlikely(ptr == 0))                                                                                                                                          \
        {                                                                                                                                                                \
            wrap_print("(null)");                                                                                                                                        \
            goto done;                                                                                                                                                   \
        }                                                                                                                                                                \
    } while (0)

    switch (peek_next)
    {
        case 'i':
        {
            shift_next;
            switch (peek_next)
            {
                case 'o':
                {
                    shift_next;
                    // io_t
                    const io_t *io = (const io_t *) ptr;
                    if (io == NULL)
                    {
                        wrap_print("(null)");
                        goto done;
                    }

                    char filepath[MOS_PATH_MAX_LENGTH];
                    io_get_name(io, filepath, sizeof(filepath));
                    wrap_print("{ '%s'", filepath);
                    if (io->closed)
                        wrap_print(", closed");
                    wrap_print(" }");
                    goto done;
                }
                default: return false;
            }

            MOS_UNREACHABLE();
        }
        case 's': // %ps
        {
            shift_next;
            // kernel symbol address
            const kallsyms_t *sym = kallsyms_get_symbol(ptr);
            if (!sym)
            {
                wrap_print("(unknown)");
                goto done;
            }

            const off_t off = ptr - sym->address;
            if (off)
                wrap_print("%s (+0x%zx)", sym ? sym->name : "(unknown)", off);
            else
                wrap_print("%s", sym ? sym->name : "(unknown)");
            goto done;
        }
        case 't': // %pt
        {
            shift_next;
            // thread_t
            null_check();
            const auto thread = (Thread *) ptr;
            if (thread == NULL)
            {
                wrap_print("(null)");
                goto done;
            }
            MOS_ASSERT_X(thread_is_valid(thread), "thread is invalid");
            wrap_print("[t%d:%s]", thread->tid, thread->name.empty() ? "<no name>" : thread->name.data());
            goto done;
        }
        case 'p': // %pp
        {
            shift_next;
            // process_t
            null_check();
            const Process *process = (const Process *) ptr;
            if (process == NULL)
            {
                wrap_print("(null)");
                goto done;
            }
            MOS_ASSERT_X(Process::IsValid(process), "process is invalid");
            wrap_print("[p%d:%s]", process->pid, process->name.empty() ? "<no name>" : process->name.data());
            goto done;
        }
        case 'v': // %pv
        {
            shift_next;

            // vmap-related types
            switch (peek_next)
            {
                case 'f': // %pvf
                {
                    shift_next;
                    // vm_flags, only r/w/x are supported
                    const vm_flags flags = *(vm_flags *) ptr;
                    wrap_printed(do_print_vmflags(buf, *size, flags));
                    goto done;
                }
                case 'm': // %pvm
                {
                    shift_next;
                    // vmap_t *, print the vmap's range and flags
                    const vmap_t *vmap = (const vmap_t *) ptr;
                    if (vmap == NULL)
                    {
                        wrap_print("(null)");
                        goto done;
                    }

                    wrap_print("{ [" PTR_VLFMT " - " PTR_VLFMT "], ", vmap->vaddr, vmap->vaddr + vmap->npages * MOS_PAGE_SIZE - 1);
                    wrap_printed(do_print_vmflags(buf, *size, vmap->vmflags));
                    wrap_print(", fault: %ps", (void *) (ptr_t) vmap->on_fault);

                    if (vmap->io)
                    {
                        char filepath[MOS_PATH_MAX_LENGTH];
                        io_get_name(vmap->io, filepath, sizeof(filepath));
                        wrap_print(", io: '%s', offset: 0x%zx", filepath, vmap->io_offset);
                    }

                    wrap_print(" }");
                    goto done;
                }
                default: return false;
            }
        }
        default: return false;
    }

done:
    return ret;
}
