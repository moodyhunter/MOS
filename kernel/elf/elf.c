// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/elf/elf.h"

#include "mos/mm/cow.h"
#include "mos/mm/mm.h"
#include "mos/mm/mmap.h"
#include "mos/mm/physical/pmm.h"
#include "mos/platform/platform.h"
#include "mos/tasks/process.h"
#include "mos/tasks/thread.h"

#include <elf.h>
#include <mos_stdlib.h>
#include <mos_string.h>
#include <stddef.h>

MOS_STATIC_ASSERT(sizeof(elf_header_t) == (MOS_BITS == 32 ? 0x34 : 0x40), "elf_header has wrong size");
MOS_STATIC_ASSERT(sizeof(elf_program_hdr_t) == (MOS_BITS == 32 ? 0x20 : 0x38), "elf_program_header has wrong size");
MOS_STATIC_ASSERT(sizeof(elf_section_hdr_t) == (MOS_BITS == 32 ? 0x28 : 0x40), "elf_section_header has wrong size");

static bool elf_verify_header(const elf_header_t *header)
{
    if (header->identity.magic[0] != '\x7f')
        return false;

    if (strncmp(&header->identity.magic[1], "ELF", 3) != 0)
        return false;

    if (header->identity.bits != ELF_BITS_MOS_DEFAULT)
        return false;

    if (header->identity.endianness != ELF_ENDIANNESS_MOS_DEFAULT)
        return false;

    if (header->identity.version != ELF_VERSION_CURRENT)
        return false;

    if (header->identity.osabi != ELF_OSABI_NONE)
        return false;

    if (header->machine_type != ELF_MACHINE_MOS_DEFAULT)
        return false;

    return true;
}

static void elf_read_file(file_t *file, void *buf, off_t offset, size_t size)
{
    const bool read = io_pread(&file->io, buf, size, offset) == size;
    MOS_ASSERT(read);
}

static bool elf_read_and_verify_executable(file_t *file, elf_header_t *header)
{
    elf_read_file(file, header, 0, sizeof(elf_header_t));
    const bool valid = elf_verify_header(header);
    if (!valid)
        return false;

    if (header->object_type != ELF_OBJTYPE_EXECUTABLE && header->object_type != ELF_OBJTYPE_SHARED_OBJECT)
        return false;

    return true;
}

static void elf_setup_main_thread(thread_t *thread, const char *command, ptr_t entry, int argc, const char *const src_argv[])
{
    mos_debug(scheduler, "cpu %d: setting up a new main thread %pt of process %pp", current_cpu->id, (void *) thread, (void *) thread->owner);

    /**
     * Typical Stack Layout:
     *
     *      (low address)
     *      |-> u32 argc
     *      |-> ptr_t argv[]
     *      |   |-> NULL
     *      |-> ptr_t envp[]
     *      |   |-> NULL
     *      |-> AuxV
     *      |   |-> AT_...
     *      |   |-> AT_NULL
     *      |-> argv strings, NULL-terminated
     *      |-> environment strings, NULL-terminated
     *      |-> u32 zero
     *      (high address, end of stack)
     */

    MOS_ASSERT_X(thread->u_stack.head == thread->u_stack.top, "thread %pt's user stack is not empty", (void *) thread);
    stack_push_val(&thread->u_stack, 0);

    const int auxv_count = 6;
    Elf64_auxv_t auxv[auxv_count];
    auxv[0].a_type = AT_PHDR;
    auxv[0].a_un.a_val = 0x40; // TODO: fixme

    auxv[1].a_type = AT_ENTRY;
    auxv[1].a_un.a_val = entry;

    auxv[2].a_type = AT_PHENT;
    auxv[2].a_un.a_val = sizeof(elf_program_hdr_t);

    auxv[3].a_type = AT_PHNUM;
    auxv[3].a_un.a_val = 5; // TODO: fixme

    stack_push(&thread->u_stack, command, strlen(command) + 1); // +1 for the null terminator
    auxv[4].a_type = AT_EXECFN;
    auxv[4].a_un.a_val = thread->u_stack.head;

    auxv[5].a_type = AT_NULL;
    auxv[5].a_un.a_val = 0;

    const int envp_count = 0;         // TODO: support envp
    const char *src_envp[envp_count]; // TODO: support envp

    void *envp[envp_count + 1]; // +1 for the null terminator
    if (envp_count == 0)
        goto no_envp;

    for (int i = envp_count - 1; i >= 0; i--)
    {
        const size_t len = strlen(src_envp[i]) + 1; // +1 for the null terminator
        stack_push(&thread->u_stack, src_envp[i], len);
        envp[i] = (void *) thread->u_stack.head;
    }

no_envp:
    envp[envp_count] = NULL;

    void *argv[argc + 1]; // +1 for the null terminator
    if (argc == 0)
        goto no_argv;
    for (int i = argc - 1; i >= 0; i--)
    {
        const size_t len = strlen(src_argv[i]) + 1; // +1 for the null terminator
        stack_push(&thread->u_stack, src_argv[i], len);
        argv[i] = (void *) thread->u_stack.head;
    }

no_argv:
    argv[argc] = NULL;

    thread->u_stack.head = ALIGN_DOWN(thread->u_stack.head, 16); // align to 16 bytes

    ptr_t user_envp, user_argv;

    stack_push(&thread->u_stack, &auxv, sizeof(Elf64_auxv_t) * auxv_count);
    stack_push(&thread->u_stack, &envp, sizeof(char *) * (envp_count + 1));
    user_envp = thread->u_stack.head;
    stack_push(&thread->u_stack, &argv, sizeof(char *) * (argc + 1));
    user_argv = thread->u_stack.head;
    stack_push_val(&thread->u_stack, argc);

    platform_context_setup_main_thread(thread, entry, thread->u_stack.head, argc, user_argv, user_envp);
}

process_t *elf_create_process(const char *path, process_t *parent, int argc, const char *const argv[], const stdio_t *ios)
{
    file_t *file = vfs_openat(FD_CWD, path, OPEN_READ | OPEN_EXECUTE);
    if (!file)
    {
        mos_warn("failed to open '%s'", path);
        return NULL;
    }

    io_ref(&file->io);

    elf_header_t elf;
    if (!elf_read_and_verify_executable(file, &elf))
    {
        pr_emerg("failed to verify ELF header for '%s'", dentry_name(file->dentry));
        return NULL;
    }

    process_t *proc = process_new(parent, file->dentry->name, ios);
    if (!proc)
    {
        mos_warn("failed to create process for '%s'", dentry_name(file->dentry));
        return NULL;
    }

    const char *new_argv[argc + 1];
    for (int i = 0; i < argc; i++)
        new_argv[i] = strdup(argv[i]); // copy the strings to kernel space, since we are switching to a new address space
    new_argv[argc] = NULL;

    mm_context_t *const prev_mm = mm_switch_context(proc->mm);

    for (int i = 0; i < elf.ph.count; i++)
    {
        elf_program_hdr_t ph;
        elf_read_file(file, &ph, elf.ph_offset + i * elf.ph.entry_size, elf.ph.entry_size);

        mos_debug(elf, "program header %d: %c%c%c, type '%d' at " PTR_FMT, //
                  i,                                                       //
                  ph.p_flags & ELF_PF_R ? 'r' : '-',                       //
                  ph.p_flags & ELF_PF_W ? 'w' : '-',                       //
                  ph.p_flags & ELF_PF_X ? 'x' : '-',                       //
                  ph.header_type,                                          //
                  ph.vaddr                                                 //
        );

        switch (ph.header_type)
        {
            case ELF_PT_NULL: break; // ignore
            case ELF_PT_INTERP:
            {
                char interp_name[ph.size_in_file];
                elf_read_file(file, interp_name, ph.data_offset, ph.size_in_file);
                pr_info("elf interpreter: %s", interp_name);
                break;
            }
            case ELF_PT_LOAD:
            {
                MOS_ASSERT_X(ph.size_in_file <= ph.size_in_mem, "invalid ELF: size in file is larger than size in memory");
                const vm_flags flags = (ph.p_flags & ELF_PF_R ? VM_READ : 0) |  //
                                       (ph.p_flags & ELF_PF_W ? VM_WRITE : 0) | //
                                       (ph.p_flags & ELF_PF_X ? VM_EXEC : 0) |  //
                                       VM_USER;

                MOS_ASSERT(ph.data_offset % MOS_PAGE_SIZE == ph.vaddr % MOS_PAGE_SIZE); // offset ≡ vaddr (mod page size)

                const size_t npages = ALIGN_UP_TO_PAGE(ph.size_in_mem) / MOS_PAGE_SIZE;
                const ptr_t aligned_vaddr = ALIGN_DOWN_TO_PAGE(ph.vaddr);
                const size_t aligned_size = ALIGN_DOWN_TO_PAGE(ph.data_offset);
                mos_debug(elf, "  mapping %zu pages at " PTR_FMT " from offset %zu", npages, aligned_vaddr, aligned_size);

                const ptr_t vaddr = mmap_file(proc->mm, aligned_vaddr, MMAP_PRIVATE | MMAP_EXACT, flags, npages, &file->io, aligned_size);
                MOS_ASSERT_X(vaddr == aligned_vaddr, "failed to map %zu pages at " PTR_FMT " from offset %zu", ph.size_in_mem / MOS_PAGE_SIZE, ph.vaddr, ph.data_offset);

                if (ph.size_in_file < ph.size_in_mem)
                {
                    mos_debug(elf, "  ... and zeroing %zu bytes at " PTR_FMT, ph.size_in_mem - ph.size_in_file, ph.vaddr + ph.size_in_file);
                    memzero((char *) ph.vaddr + ph.size_in_file, ph.size_in_mem - ph.size_in_file);
                }

                mos_debug(elf, "  ... done");
                break;
            }
            case ELF_PT_NOTE: break;    // intentionally ignored
            case ELF_PT_DYNAMIC: break; // will be handled by the dynamic linker
            case ELF_PT_PHDR:
            case ELF_PT_TLS:
            case ELF_PT_SHLIB:
            {
                pr_warn("ignoring unsupported program header type %d", ph.header_type);
                break;
            }
            default:
            {
                if (IN_RANGE(ph.header_type, ELF_PT_OS_LOW, ELF_PT_OS_HIGH))
                    mos_debug(elf, "ignoring OS-specific program header type 0x%x", ph.header_type);
                else if (IN_RANGE(ph.header_type, ELF_PT_PROCESSOR_LO, ELF_PT_PROCESSOR_HI))
                    mos_debug(elf, "ignoring processor-specific program header type 0x%x", ph.header_type);
                else
                    pr_warn("unknown program header type 0x%x", ph.header_type);
                break;
            }
        };
    }

    elf_setup_main_thread(proc->main_thread, path, elf.entry_point, argc, new_argv);
    thread_complete_init(proc->main_thread);
    mm_switch_context(prev_mm);

    for (int i = 0; i < argc; i++)
        kfree((void *) new_argv[i]);

    if (file)
        io_unref(&file->io); // close the file, we should have the file's refcount == 0 here

    return proc;
}
