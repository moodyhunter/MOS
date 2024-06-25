// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/elf.h"

#include "mos/filesystem/vfs.h"
#include "mos/mm/mmap.h"
#include "mos/platform/platform.h"
#include "mos/syslog/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_types.h"
#include "mos/tasks/thread.h"

#include <mos/types.h>
#include <mos_stdlib.h>
#include <mos_string.h>

MOS_STATIC_ASSERT(sizeof(elf_header_t) == 0x40, "elf_header has wrong size");
MOS_STATIC_ASSERT(sizeof(elf_program_hdr_t) == 0x38, "elf_program_header has wrong size");
MOS_STATIC_ASSERT(sizeof(elf_section_hdr_t) == 0x40, "elf_section_header has wrong size");

static void add_auxv_entry(auxv_vec_t *var, unsigned long type, unsigned long val)
{
    MOS_ASSERT_X(var->count < AUXV_VEC_SIZE, "auxv vector overflow, increase AUXV_VEC_SIZE");

    var->vector[var->count].a_type = type;
    var->vector[var->count].a_un.a_val = val;
    var->count++;
}

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

[[nodiscard]] static bool elf_read_file(file_t *file, void *buf, off_t offset, size_t size)
{
    const size_t read = io_pread(&file->io, buf, size, offset);
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

static void elf_setup_main_thread(thread_t *thread, elf_startup_info_t *const info, ptr_t *const out_pargv, ptr_t *const out_penvp)
{
    pr_dinfo2(elf, "cpu %d: setting up a new main thread %pt of process %pp", current_cpu->id, (void *) thread, (void *) thread->owner);

    MOS_ASSERT_X(thread->u_stack.head == thread->u_stack.top, "thread %pt's user stack is not empty", (void *) thread);
    stack_push_val(&thread->u_stack, (uintn) 0);

    const void *stack_envp[info->envc + 1]; // +1 for the null terminator
    const void *stack_argv[info->argc + 1]; // +1 for the null terminator

    // calculate the size of entire stack usage
    size_t stack_size = 0;
    stack_size += sizeof(uintn);                // the topmost zero
    stack_size += strlen(info->invocation) + 1; // +1 for the null terminator

    for (int i = 0; i < info->envc; i++)
        stack_size += strlen(info->envp[i]) + 1; // +1 for the null terminator

    for (int i = 0; i < info->argc; i++)
        stack_size += strlen(info->argv[i]) + 1; // +1 for the null terminator

    stack_size += sizeof(Elf64_auxv_t) * (info->auxv.count + 2); // AT_EXECFN and AT_NULL
    stack_size += sizeof(stack_envp);                            // envp
    stack_size += sizeof(stack_argv);                            // argv
    stack_size += sizeof(uintn);                                 // argc

    // align to 16 bytes
    const size_t aligned_stack_size = ALIGN_UP(stack_size, 16);
    thread->u_stack.head = thread->u_stack.top - (aligned_stack_size - stack_size); // so that the stack can be aligned to 16 bytes

    stack_push_val(&thread->u_stack, (uintn) 0);

    void *invocation_ptr = stack_push(&thread->u_stack, info->invocation, strlen(info->invocation) + 1); // +1 for the null terminator

    add_auxv_entry(&info->auxv, AT_EXECFN, (ptr_t) invocation_ptr);
    add_auxv_entry(&info->auxv, AT_NULL, 0);

    // ! copy the environment to the stack in reverse order !
    if (info->envc == 0)
        goto no_envp;

    for (int i = info->envc - 1; i >= 0; i--)
    {
        const size_t len = strlen(info->envp[i]) + 1; // +1 for the null terminator
        stack_envp[i] = stack_push(&thread->u_stack, info->envp[i], len);
    }

no_envp:
    stack_envp[info->envc] = NULL;

    // ! copy the argv to the stack in reverse order !
    if (info->argc == 0)
        goto no_argv;

    for (int i = info->argc - 1; i >= 0; i--)
    {
        const size_t len = strlen(info->argv[i]) + 1; // +1 for the null terminator
        stack_argv[i] = stack_push(&thread->u_stack, info->argv[i], len);
    }

no_argv:
    stack_argv[info->argc] = NULL;

    stack_push(&thread->u_stack, info->auxv.vector, sizeof(Elf64_auxv_t) * info->auxv.count);          // auxv
    *out_penvp = (ptr_t) stack_push(&thread->u_stack, &stack_envp, sizeof(char *) * (info->envc + 1)); // envp
    *out_pargv = (ptr_t) stack_push(&thread->u_stack, &stack_argv, sizeof(char *) * (info->argc + 1)); // argv
    stack_push_val(&thread->u_stack, (uintn) info->argc);                                              // argc
    MOS_ASSERT(thread->u_stack.head % 16 == 0);
}

static void elf_map_segment(const elf_program_hdr_t *const ph, ptr_t map_bias, mm_context_t *mm, file_t *file)
{
    MOS_ASSERT(ph->header_type == ELF_PT_LOAD);
    pr_dinfo2(elf, "program header %c%c%c, type '%d' at " PTR_FMT, //
              ph->p_flags & ELF_PF_R ? 'r' : '-',                  //
              ph->p_flags & ELF_PF_W ? 'w' : '-',                  //
              ph->p_flags & ELF_PF_X ? 'x' : '-',                  //
              ph->header_type,                                     //
              ph->vaddr                                            //
    );

    MOS_ASSERT(ph->data_offset % MOS_PAGE_SIZE == ph->vaddr % MOS_PAGE_SIZE); // offset ≡ vaddr (mod page size)
    MOS_ASSERT_X(ph->size_in_file <= ph->size_in_mem, "invalid ELF: size in file is larger than size in memory");

    const vm_flags flags = VM_USER | (ph->p_flags & ELF_PF_R ? VM_READ : 0) | (ph->p_flags & ELF_PF_W ? VM_WRITE : 0) | (ph->p_flags & ELF_PF_X ? VM_EXEC : 0);
    const ptr_t aligned_vaddr = ALIGN_DOWN_TO_PAGE(ph->vaddr);
    const size_t npages = (ALIGN_UP_TO_PAGE(ph->vaddr + ph->size_in_mem) - aligned_vaddr) / MOS_PAGE_SIZE;
    const size_t aligned_size = ALIGN_DOWN_TO_PAGE(ph->data_offset);

    const ptr_t map_start = map_bias + aligned_vaddr;
    pr_dinfo2(elf, "  mapping %zu pages at " PTR_FMT " (bias at " PTR_FMT ") from offset %zu...", npages, map_start, map_bias, aligned_size);

    const ptr_t vaddr = mmap_file(mm, map_start, MMAP_PRIVATE | MMAP_EXACT, flags, npages, &file->io, aligned_size);
    MOS_ASSERT_X(vaddr == map_start, "failed to map ELF segment at " PTR_FMT, aligned_vaddr);

    if (ph->size_in_file < ph->size_in_mem)
    {
        pr_dinfo2(elf, "  ... and zeroing %zu bytes at " PTR_FMT, ph->size_in_mem - ph->size_in_file, map_bias + ph->vaddr + ph->size_in_file);
        memzero((char *) map_bias + ph->vaddr + ph->size_in_file, ph->size_in_mem - ph->size_in_file);
    }

    pr_dinfo2(elf, "  ... done");
}

static ptr_t elf_map_interpreter(const char *path, mm_context_t *mm)
{
    file_t *const interp_file = vfs_openat(AT_FDCWD, path, OPEN_READ | OPEN_EXECUTE);
    if (IS_ERR(interp_file))
        return 0;

    io_ref(&interp_file->io);

    elf_header_t elf;
    if (!elf_read_and_verify_executable(interp_file, &elf))
    {
        pr_emerg("failed to verify ELF header for '%s'", dentry_name(interp_file->dentry));
        io_unref(&interp_file->io);
        return 0;
    }

    ptr_t entry = 0;

    for (size_t i = 0; i < elf.ph.count; i++)
    {
        elf_program_hdr_t ph;
        if (!elf_read_file(interp_file, &ph, elf.ph_offset + i * elf.ph.entry_size, elf.ph.entry_size))
        {
            pr_emerg("failed to read program header %zu for '%s'", i, dentry_name(interp_file->dentry));
            io_unref(&interp_file->io);
            return 0;
        }

        if (ph.header_type == ELF_PT_LOAD)
        {
            // interpreter is always loaded at vaddr 0
            elf_map_segment(&ph, MOS_ELF_INTERPRETER_BASE_OFFSET, mm, interp_file);
            entry = elf.entry_point;
        }
    }

    io_unref(&interp_file->io);
    return MOS_ELF_INTERPRETER_BASE_OFFSET + entry;
}

__nodiscard bool elf_do_fill_process(process_t *proc, file_t *file, elf_header_t elf, elf_startup_info_t *info)
{
    bool ret = true;

    add_auxv_entry(&info->auxv, AT_PAGESZ, MOS_PAGE_SIZE);
    add_auxv_entry(&info->auxv, AT_UID, 0);
    add_auxv_entry(&info->auxv, AT_EUID, 0);
    add_auxv_entry(&info->auxv, AT_GID, 0);
    add_auxv_entry(&info->auxv, AT_EGID, 0);
    add_auxv_entry(&info->auxv, AT_BASE, MOS_ELF_INTERPRETER_BASE_OFFSET);

    // !! after this point, we must make sure that we switch back to the previous address space before returning from this function !!
    mm_context_t *const prev_mm = mm_switch_context(proc->mm);

    bool should_bias = elf.object_type == ET_DYN; // only ET_DYN (shared libraries) needs randomization
    ptrdiff_t map_bias = 0;                       // ELF segments are loaded at vaddr + load_bias

    bool has_interpreter = false;
    ptr_t interp_entrypoint = 0;
    ptr_t auxv_phdr_vaddr = false; // whether we need to add AT_PHDR, AT_PHENT, AT_PHNUM to the auxv vector

    for (size_t i = 0; i < elf.ph.count; i++)
    {
        elf_program_hdr_t ph;
        if (!elf_read_file(file, &ph, elf.ph_offset + i * elf.ph.entry_size, elf.ph.entry_size))
        {
            pr_emerg("failed to read program header %zu for '%s'", i, dentry_name(file->dentry));
            goto bad_proc;
        }

        switch (ph.header_type)
        {
            case ELF_PT_NULL: break; // ignore
            case ELF_PT_INTERP:
            {
                char interp_name[ph.size_in_file];
                if (!elf_read_file(file, interp_name, ph.data_offset, ph.size_in_file))
                {
                    pr_emerg("failed to read interpreter name for '%s'", dentry_name(file->dentry));
                    goto bad_proc;
                }
                pr_dinfo2(elf, "elf interpreter: %s", interp_name);
                has_interpreter = true;
                interp_entrypoint = elf_map_interpreter(interp_name, proc->mm);
                if (!interp_entrypoint)
                {
                    pr_dinfo2(elf, "failed to map interpreter '%s'", interp_name);
                    goto bad_proc;
                }

                if (should_bias)
                    map_bias = elf_determine_loadbias(&elf);

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
                    pr_dinfo2(elf, "ignoring OS-specific program header type 0x%x", ph.header_type);
                else if (MOS_IN_RANGE(ph.header_type, ELF_PT_PROCESSOR_LO, ELF_PT_PROCESSOR_HI))
                    pr_dinfo2(elf, "ignoring processor-specific program header type 0x%x", ph.header_type);
                else
                    pr_warn("unknown program header type 0x%x", ph.header_type);
                break;
            }
        };
    }

    if (auxv_phdr_vaddr)
    {
        add_auxv_entry(&info->auxv, AT_PHDR, map_bias + auxv_phdr_vaddr);
        add_auxv_entry(&info->auxv, AT_PHENT, elf.ph.entry_size);
        add_auxv_entry(&info->auxv, AT_PHNUM, elf.ph.count);
    }

    add_auxv_entry(&info->auxv, AT_ENTRY, map_bias + elf.entry_point); // the entry point of the executable, not the interpreter

    ptr_t user_argv, user_envp;
    thread_t *const main_thread = proc->main_thread;
    elf_setup_main_thread(main_thread, info, &user_argv, &user_envp);
    platform_context_setup_main_thread(main_thread, has_interpreter ? interp_entrypoint : elf.entry_point, main_thread->u_stack.head, info->argc, user_argv, user_envp);

    goto done;

bad_proc:
    ret = false;

done:;
    mm_context_t *prev = mm_switch_context(prev_mm);
    MOS_UNUSED(prev);

    return ret;
}

bool elf_read_and_verify_executable(file_t *file, elf_header_t *header)
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

bool elf_fill_process(process_t *proc, file_t *file, const char *path, const char *const argv[], const char *const envp[])
{
    bool ret = false;

    io_ref(&file->io);

    elf_header_t elf;
    if (!elf_read_and_verify_executable(file, &elf))
    {
        pr_emerg("failed to verify ELF header for '%s'", dentry_name(file->dentry));
        goto cleanup_close_file;
    }

    int argc = 0;
    while (argv && argv[argc] != NULL)
        argc++;

    int envc = 0;
    while (envp && envp[envc] != NULL)
        envc++;

    elf_startup_info_t info = {
        .invocation = strdup(path),
        .argc = argc,
        .argv = kmalloc(sizeof(char *) * (argc + 1)),
        .envc = envc,
        .envp = kmalloc(sizeof(char *) * (envc + 1)),
        .auxv = { 0 },
    };

    for (int i = 0; i < argc; i++)
        info.argv[i] = strdup(argv[i]); // copy the strings to kernel space, since we are switching to a new address space
    info.argv[argc] = NULL;

    for (int i = 0; i < envc; i++)
        info.envp[i] = strdup(envp[i]); // copy the strings to kernel space, since we are switching to a new address space
    info.envp[envc] = NULL;

    ret = elf_do_fill_process(proc, file, elf, &info);

    if (info.invocation)
        kfree(info.invocation);
    for (int i = 0; i < argc; i++)
        kfree(info.argv[i]);
    for (int i = 0; i < envc; i++)
        kfree(info.envp[i]);
    kfree(info.argv);
    kfree(info.envp);

cleanup_close_file:
    io_unref(&file->io); // close the file, we should have the file's refcount == 0 here

    return ret;
}

process_t *elf_create_process(const char *path, process_t *parent, const char *const argv[], const char *const envp[], const stdio_t *ios)
{
    process_t *proc = NULL;
    file_t *file = vfs_openat(AT_FDCWD, path, OPEN_READ | OPEN_EXECUTE);
    if (IS_ERR(file))
    {
        mos_warn("failed to open '%s'", path);
        return NULL;
    }
    io_ref(&file->io);

    proc = process_new(parent, file->dentry->name, ios);
    if (!proc)
    {
        mos_warn("failed to create process for '%s'", dentry_name(file->dentry));
        goto cleanup_close_file;
    }

    const bool filled = elf_fill_process(proc, file, path, argv, envp);
    thread_complete_init(proc->main_thread);

    if (!filled)
    {
        // TODO how do we make sure that the process is cleaned up properly?
        process_handle_exit(proc, 0, SIGKILL);
        proc = NULL;
    }

cleanup_close_file:
    io_unref(&file->io); // close the file, we should have the file's refcount == 0 here
    return proc;
}
