// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/kallsyms.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"

#include <mos/types.h>
#include <mos_stdio.h>

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
 *              e.g.: "'my_process' (pid 123)"
 *  '%pp'   prints a process_t object.
 *              e.g.: "'my_thread' (tid 123)"
 *  '%pvf'  prints vm_flags_t flag, only the r/w/x bits are printed for general purpose.
 *              e.g.: "rwx" / "r--" / "rw-" / "--x"
 *
 * @returns true if the format specifier is handled, false otherwise, the
 *          caller should fallback to the default implementation. (i.e. %p)
 *
 */

size_t vsnprintf_do_pointer_kernel(char *buf, size_t *size, const char **pformat, ptr_t ptr)
{
    size_t ret = 0;
#define current         (**pformat)
#define peek_next       (*(*pformat + 1))
#define shift_next      ((void) ((*pformat)++))
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
        case 's':
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
        case 't':
        {
            shift_next;
            // thread_t
            null_check();
            const thread_t *thread = (const thread_t *) ptr;
            wrap_print("'%s' (tid %d)", thread->name ? thread->name : "<no name>", thread->tid);
            goto done;
        }
        case 'p':
        {
            shift_next;
            // process_t
            null_check();
            const process_t *process = (const process_t *) ptr;
            wrap_print("'%s' (pid %d)", process->name ? process->name : "<no name>", process->pid);
            goto done;
        }
        case 'v':
        {
            shift_next;

            // vmap-related types
            switch (peek_next)
            {
                case 'f':
                {
                    shift_next;
                    // vm_flags, only r/w/x are supported
                    const vm_flags flags = *(vm_flags *) ptr;
                    wrap_printed(do_print_vmflags(buf, *size, flags));
                    goto done;
                }
                case 'm':
                {
                    shift_next;
                    // vmap_t *, print the vmap's range and flags
                    const vmap_t *vmap = (const vmap_t *) ptr;
                    wrap_print("{ " PTR_RANGE ", ", vmap->vaddr, vmap->vaddr + vmap->npages * MOS_PAGE_SIZE - 1);
                    wrap_printed(do_print_vmflags(buf, *size, vmap->vmflags));
                    wrap_print(", on_fault=%ps", (void *) (ptr_t) vmap->on_fault);
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
