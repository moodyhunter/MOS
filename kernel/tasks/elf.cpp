// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/elf.hpp"

#include "mos/filesystem/vfs.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/mmap.hpp"
#include "mos/platform/platform.hpp"
#include "mos/tasks/process.hpp"
#include "mos/tasks/schedule.hpp"
#include "mos/tasks/task_types.hpp"
#include "mos/tasks/thread.hpp"

#include <elf.h>
#include <mos/types.hpp>
#include <mos/vector.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

MOS_STATIC_ASSERT(sizeof(elf_header_t) == 0x40, "elf_header has wrong size");
MOS_STATIC_ASSERT(sizeof(elf_program_hdr_t) == 0x38, "elf_program_header has wrong size");

static bool elf_verify_header(const elf_header_t *header)
{
    if (header->identity.magic[0] != ELFMAG0)
        return false;

    if (strncmp(&header->identity.magic[1], "ELF", 3) != 0)
        return false;

    if (header->identity.bits != ELFCLASS64)
        return false;

    if (header->identity.endianness != ELF_ENDIANNESS_MOS_DEFAULT)
        return false;

    if (header->identity.osabi != 0)
        return false;

    if (header->identity.version != EV_CURRENT)
        return false;

    if (header->machine_type != MOS_ELF_PLATFORM)
        return false;

    return true;
}

[[nodiscard]] static bool elf_read_file(FsBaseFile *file, void *buf, off_t offset, size_t size)
{
    const size_t read = file->pread(buf, size, offset);
    return read == size;
}

static ptr_t elf_determine_loadbias(elf_header_t *elf)
{
    MOS_UNUSED(elf);
    return 0x4000000; // TODO: randomize
}

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

static void elf_setup_main_thread(Thread *thread, elf_startup_info_t *const info, ptr_t *const out_pargv, ptr_t *const out_penvp)
{
    dInfo2<elf> << "cpu " << current_cpu->id << ": setting up a new main thread " << thread << " of process " << thread->owner;

    MOS_ASSERT_X(thread->u_stack.head == thread->u_stack.top, "thread %pt's user stack is not empty", thread);
    stack_push_val(&thread->u_stack, (uintn) 0);

    const void *stack_envp[info->envp.size() + 1]; // +1 for the null terminator
    const void *stack_argv[info->argv.size() + 1]; // +1 for the null terminator

    // calculate the size of entire stack usage
    size_t stack_size = 0;
    stack_size += sizeof(uintn);               // the topmost zero
    stack_size += info->invocation.size() + 1; // +1 for the null terminator

    for (const auto &env : info->envp)
        stack_size += env.size() + 1; // +1 for the null terminator

    for (const auto &arg : info->argv)
        stack_size += arg.size() + 1; // +1 for the null terminator

    stack_size += sizeof(Elf64_auxv_t) * (info->auxv.size() + 2); // AT_EXECFN and AT_NULL
    stack_size += sizeof(stack_envp);                             // envp
    stack_size += sizeof(stack_argv);                             // argv
    stack_size += sizeof(uintn);                                  // argc

    // align to 16 bytes
    const size_t aligned_stack_size = ALIGN_UP(stack_size, 16);
    thread->u_stack.head = thread->u_stack.top - (aligned_stack_size - stack_size); // so that the stack can be aligned to 16 bytes

    stack_push_val(&thread->u_stack, (uintn) 0);

    void *invocation_ptr = stack_push(&thread->u_stack, info->invocation.c_str(), info->invocation.size() + 1); // +1 for the null terminator

    info->AddAuxvEntry(AT_EXECFN, (ptr_t) invocation_ptr);
    info->AddAuxvEntry(AT_NULL, 0);

    // ! copy the environment to the stack in reverse order !
    if (info->envp.empty())
        goto no_envp;

    for (int i = info->envp.size() - 1; i >= 0; i--)
    {
        const size_t len = info->envp[i].size() + 1; // +1 for the null terminator
        stack_envp[i] = stack_push(&thread->u_stack, info->envp[i].c_str(), len);
    }

no_envp:
    stack_envp[info->envp.size()] = NULL;

    // ! copy the argv to the stack in reverse order !
    if (info->argv.empty())
        goto no_argv;

    for (int i = info->argv.size() - 1; i >= 0; i--)
    {
        const size_t len = info->argv[i].size() + 1; // +1 for the null terminator
        stack_argv[i] = stack_push(&thread->u_stack, info->argv[i].c_str(), len);
    }

no_argv:
    stack_argv[info->argv.size()] = NULL;

    stack_push(&thread->u_stack, info->auxv.data(), sizeof(Elf64_auxv_t) * info->auxv.size());                // auxv
    *out_penvp = (ptr_t) stack_push(&thread->u_stack, &stack_envp, sizeof(char *) * (info->envp.size() + 1)); // envp
    *out_pargv = (ptr_t) stack_push(&thread->u_stack, &stack_argv, sizeof(char *) * (info->argv.size() + 1)); // argv
    stack_push_val(&thread->u_stack, (uintn) info->argv.size());                                              // argc
    MOS_ASSERT(thread->u_stack.head % 16 == 0);
}

static void elf_map_segment(const elf_program_hdr_t *const ph, ptr_t map_bias, MMContext *mm, FsBaseFile *file)
{
    MOS_ASSERT(ph->header_type == ELF_PT_LOAD);
    dInfo2<elf> << "program header "                    //
                << (ph->flags() & ELF_PF_R ? 'r' : '-') //
                << (ph->flags() & ELF_PF_W ? 'w' : '-') //
                << (ph->flags() & ELF_PF_X ? 'x' : '-') //
                << ", type '" << ph->header_type << "' at " << ph->vaddr;

    MOS_ASSERT(ph->data_offset % MOS_PAGE_SIZE == ph->vaddr % MOS_PAGE_SIZE); // offset ≡ vaddr (mod page size)
    MOS_ASSERT_X(ph->size_in_file <= ph->size_in_mem, "invalid ELF: size in file is larger than size in memory");

    const VMFlags flags = [pflags = ph->flags()]()
    {
        VMFlags f = VM_USER;
        if (pflags & ELF_PF_R)
            f |= VM_READ;
        if (pflags & ELF_PF_W)
            f |= VM_WRITE;
        if (pflags & ELF_PF_X)
            f |= VM_EXEC;
        return f;
    }();

    const ptr_t aligned_vaddr = ALIGN_DOWN_TO_PAGE(ph->vaddr);
    const size_t npages = (ALIGN_UP_TO_PAGE(ph->vaddr + ph->size_in_mem) - aligned_vaddr) / MOS_PAGE_SIZE;
    const size_t aligned_size = ALIGN_DOWN_TO_PAGE(ph->data_offset);

    const ptr_t map_start = map_bias + aligned_vaddr;
    dInfo2<elf> << "  mapping " << npages << " pages at " << map_start << " (bias at " << map_bias << ") from offset " << aligned_size << "...";

    const ptr_t vaddr = mmap_file(mm, map_start, MMAP_PRIVATE | MMAP_EXACT, flags, npages, file, aligned_size);
    MOS_ASSERT_X(vaddr == map_start, "failed to map ELF segment at " PTR_FMT, aligned_vaddr);

    if (ph->size_in_file < ph->size_in_mem)
    {
        dInfo2<elf> << "  ... and zeroing " << (ph->size_in_mem - ph->size_in_file) << " bytes at " << (map_bias + ph->vaddr + ph->size_in_file);
        memzero((char *) map_bias + ph->vaddr + ph->size_in_file, ph->size_in_mem - ph->size_in_file);
    }

    dInfo2<elf> << "  ... done";
}

static ptr_t elf_map_interpreter(const char *path, MMContext *mm)
{
    auto interp_file = vfs_openat(AT_FDCWD, path, OPEN_READ | OPEN_EXECUTE);
    if (interp_file.isErr())
        return 0;

    interp_file->ref();

    elf_header_t elf;
    if (!elf_read_and_verify_executable(interp_file.get(), &elf))
    {
        mEmerg << "failed to verify ELF header for '" << dentry_name(interp_file->dentry) << "'";
        interp_file->unref();
        return 0;
    }

    ptr_t entry = 0;

    for (size_t i = 0; i < elf.ph.count; i++)
    {
        elf_program_hdr_t ph;
        if (!elf_read_file(interp_file.get(), &ph, elf.ph_offset + i * elf.ph.entry_size, elf.ph.entry_size))
        {
            mEmerg << "failed to read program header " << i << " for '" << dentry_name(interp_file->dentry) << "'";
            interp_file->unref();
            return 0;
        }

        if (ph.header_type == ELF_PT_LOAD)
        {
            // interpreter is always loaded at vaddr 0
            elf_map_segment(&ph, MOS_ELF_INTERPRETER_BASE_OFFSET, mm, interp_file.get());
            entry = elf.entry_point;
        }
    }

    interp_file->unref();
    return MOS_ELF_INTERPRETER_BASE_OFFSET + entry;
}

__nodiscard bool elf_do_fill_process(Process *proc, FsBaseFile *file, elf_header_t header, elf_startup_info_t *info)
{
    bool ret = true;

    info->AddAuxvEntry(AT_PAGESZ, MOS_PAGE_SIZE);
    info->AddAuxvEntry(AT_UID, 0);
    info->AddAuxvEntry(AT_EUID, 0);
    info->AddAuxvEntry(AT_GID, 0);
    info->AddAuxvEntry(AT_EGID, 0);
    info->AddAuxvEntry(AT_BASE, MOS_ELF_INTERPRETER_BASE_OFFSET);

    // !! after this point, we must make sure that we switch back to the previous address space before returning from this function !!
    MMContext *const prev_mm = mm_switch_context(proc->mm);

    bool should_bias = header.object_type == ET_DYN; // only ET_DYN (shared libraries) needs randomization
    ptrdiff_t map_bias = 0;                          // ELF segments are loaded at vaddr + load_bias

    bool has_interpreter = false;
    ptr_t interp_entrypoint = 0;
    ptr_t auxv_phdr_vaddr = false; // whether we need to add AT_PHDR, AT_PHENT, AT_PHNUM to the auxv vector

    for (size_t i = 0; i < header.ph.count; i++)
    {
        elf_program_hdr_t ph;
        if (!elf_read_file(file, &ph, header.ph_offset + i * header.ph.entry_size, header.ph.entry_size))
        {
            mEmerg << "failed to read program header " << i << " for '" << dentry_name(file->dentry) << "'";
            const auto prev = mm_switch_context(prev_mm);
            (void) prev;
            return false;
        }

        switch (ph.header_type)
        {
            case ELF_PT_NULL: break; // ignore
            case ELF_PT_INTERP:
            {
                char interp_name[ph.size_in_file];
                if (!elf_read_file(file, interp_name, ph.data_offset, ph.size_in_file))
                {
                    mEmerg << "failed to read interpreter name for '" << dentry_name(file->dentry) << "'";
                    const auto prev = mm_switch_context(prev_mm);
                    (void) prev;
                    return false;
                }
                dInfo2<elf> << "elf interpreter: " << interp_name;
                has_interpreter = true;
                interp_entrypoint = elf_map_interpreter(interp_name, proc->mm);
                if (!interp_entrypoint)
                {
                    dInfo2<elf> << "failed to map interpreter '" << interp_name << "'";
                    const auto prev = mm_switch_context(prev_mm);
                    (void) prev;
                    return false;
                }

                if (should_bias)
                    map_bias = elf_determine_loadbias(&header);

                break;
            }
            case ELF_PT_LOAD:
            {
                elf_map_segment(&ph, map_bias, proc->mm, file);
                break;
            }
            case ELF_PT_PHDR:
            {
                auxv_phdr_vaddr = ph.vaddr;
                break;
            }

            case ELF_PT_NOTE: break;    // intentionally ignored
            case ELF_PT_DYNAMIC: break; // will be handled by the dynamic linker
            case ELF_PT_TLS: break;     // will be handled by the dynamic linker or libc
            default:
            {
                if (MOS_IN_RANGE(ph.header_type, ELF_PT_OS_LOW, ELF_PT_OS_HIGH))
                    dInfo2<elf> << "ignoring OS-specific program header type 0x" << ph.header_type;
                else if (MOS_IN_RANGE(ph.header_type, ELF_PT_PROCESSOR_LO, ELF_PT_PROCESSOR_HI))
                    dInfo2<elf> << "ignoring processor-specific program header type 0x" << ph.header_type;
                else
                    mWarn << "unknown program header type 0x" << ph.header_type;
                break;
            }
        };
    }

    if (auxv_phdr_vaddr)
    {
        info->AddAuxvEntry(AT_PHDR, map_bias + auxv_phdr_vaddr);
        info->AddAuxvEntry(AT_PHENT, header.ph.entry_size);
        info->AddAuxvEntry(AT_PHNUM, header.ph.count);
    }

    info->AddAuxvEntry(AT_ENTRY, map_bias + header.entry_point); // the entry point of the executable, not the interpreter

    ptr_t user_argv, user_envp;
    const auto main_thread = proc->main_thread;
    elf_setup_main_thread(main_thread, info, &user_argv, &user_envp);
    platform_context_setup_main_thread(                           //
        main_thread,                                              //
        has_interpreter ? interp_entrypoint : header.entry_point, //
        main_thread->u_stack.head,                                //
        info->argv.size(),                                        //
        user_argv,                                                //
        user_envp                                                 //
    );

    MMContext *prev = mm_switch_context(prev_mm);
    MOS_UNUSED(prev);

    return ret;
}

bool elf_read_and_verify_executable(FsBaseFile *file, elf_header_t *header)
{
    if (!elf_read_file(file, header, 0, sizeof(elf_header_t)))
        return false;

    const bool valid = elf_verify_header(header);
    if (!valid)
        return false;

    if (header->object_type != ET_EXEC && header->object_type != ET_DYN)
        return false;

    return true;
}

[[nodiscard]] static bool elf_fill_process(Process *proc, FsBaseFile *file, const char *path, const char *const argv[], const char *const envp[])
{
    bool ret = false;

    file->ref();

    elf_header_t elf;
    if (!elf_read_and_verify_executable(file, &elf))
    {
        mEmerg << "failed to verify ELF header for '" << dentry_name(file->dentry) << "'";
        file->unref(); // close the file, we should have the file's refcount == 0 here
        return ret;
    }

    elf_startup_info_t info{ .invocation = path };

    while (argv && argv[info.argv.size()])
        info.argv.push_back(argv[info.argv.size()]);

    if (info.argv.empty())
        info.argv = { path };

    while (envp && envp[info.envp.size()])
        info.envp.push_back(envp[info.envp.size()]);

    ret = elf_do_fill_process(proc, file, elf, &info);

    file->unref(); // close the file, we should have the file's refcount == 0 here
    return ret;
}

Process *elf_create_process(const char *path, Process *parent, const char *const argv[], const char *const envp[], const stdio_t *ios)
{
    auto file = vfs_openat(AT_FDCWD, path, OPEN_READ | OPEN_EXECUTE);
    if (file.isErr())
    {
        mos_warn("failed to open '%s'", path);
        return NULL;
    }
    file->ref();

    auto proc = process_new(parent, file->dentry->name, ios);
    if (!proc)
    {
        mos_warn("failed to create process for '%s'", dentry_name(file->dentry).c_str());
        file->unref();
        return proc;
    }

    const bool filled = elf_fill_process(proc, file.get(), path, argv, envp);
    thread_complete_init(proc->main_thread);
    scheduler_add_thread(proc->main_thread);

    if (!filled)
    {
        // TODO how do we make sure that the process is cleaned up properly?
        process_exit(std::move(proc), 0, SIGKILL);
        proc = NULL;
    }

    file->unref(); // close the file, we should have the file's refcount == 0 here
    return proc;
}
