// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/kallsyms.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"

#include <mos/types.h>
#include <stdio.h>

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

bool vsnprintf_do_pointer_kernel(char **buf, size_t *size, const char **pformat, ptr_t ptr)
{
#define current    (**pformat)
#define peek_next  (*(*pformat + 1))
#define shift_next ((void) ((*pformat)++))

    if (current != 'p')
        return false;

#define null_check()                                                                                                                                                     \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (unlikely(ptr == 0))                                                                                                                                          \
        {                                                                                                                                                                \
            *buf += snprintf(*buf, *size, "(null)");                                                                                                                     \
            return true;                                                                                                                                                 \
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
                *buf += snprintf(*buf, *size, "(unknown)");
                return true;
            }

            const off_t off = ptr - sym->address;
            if (off)
                *buf += snprintf(*buf, *size, "%s (+%zx)", sym ? sym->name : "(unknown)", off);
            else
                *buf += snprintf(*buf, *size, "%s", sym ? sym->name : "(unknown)");
            return true;
        }
        case 't':
        {
            shift_next;
            // thread_t
            null_check();
            const thread_t *thread = (const thread_t *) ptr;
            *buf += snprintf(*buf, *size, "'%s' (tid %d)", thread->name ? thread->name : "<no name>", thread->tid);
            return true;
        }
        case 'p':
        {
            shift_next;
            // process_t
            null_check();
            const process_t *process = (const process_t *) ptr;
            *buf += snprintf(*buf, *size, "'%s' (pid %d)", process->name ? process->name : "<no name>", process->pid);
            return true;
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
                    *buf += snprintf(*buf, *size, "%c%c%c", (flags & VM_READ) ? 'r' : '-', (flags & VM_WRITE) ? 'w' : '-', (flags & VM_EXEC) ? 'x' : '-');
                    return true;
                }
                default: return false;
            }
        }
        default: return false;
    }
}
