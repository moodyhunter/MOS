// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/memops.h"
#include "mos/mm/mm.h"
#include "mos/mm/paging/table_ops.h"

#include <mos/elf/elf.h>
#include <mos/filesystem/vfs.h>
#include <mos/kconfig.h>
#include <mos/mm/cow.h>
#include <mos/mm/paging/paging.h>
#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/process.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <mos/types.h>
#include <string.h>

MOS_STATIC_ASSERT(sizeof(elf_header_t) == (MOS_BITS == 32 ? 0x34 : 0x40), "elf_header has wrong size");
MOS_STATIC_ASSERT(sizeof(elf_program_hdr_t) == (MOS_BITS == 32 ? 0x20 : 0x38), "elf_program_header has wrong size");
MOS_STATIC_ASSERT(sizeof(elf_section_hdr_t) == (MOS_BITS == 32 ? 0x28 : 0x40), "elf_section_header has wrong size");

static const char *elf_program_header_type_str[_ELF_PT_COUNT] = {
    [ELF_PT_NULL] = "NULL", [ELF_PT_LOAD] = "LOAD",   [ELF_PT_DYNAMIC] = "DYNAMIC", [ELF_PT_INTERP] = "INTERP",
    [ELF_PT_NOTE] = "NOTE", [ELF_PT_SHLIB] = "SHLIB", [ELF_PT_PHDR] = "PHDR",       [ELF_PT_TLS] = "TLS",
};

elf_verify_result elf_verify_header(const elf_header_t *header)
{
    if (header->identity.magic[0] != '\x7f')
        return ELF_VERIFY_INVALID_MAGIC;

    if (strncmp(&header->identity.magic[1], "ELF", 3) != 0)
        return ELF_VERIFY_INVALID_MAGIC_ELF;

    if (header->identity.bits != ELF_BITS_MOS_DEFAULT)
        return ELF_VERIFY_INVALID_BITS;

    if (header->identity.endianness != ELF_ENDIANNESS_MOS_DEFAULT)
        return ELF_VERIFY_INVALID_ENDIAN;

    if (header->identity.version != ELF_VERSION_CURRENT)
        return ELF_VERIFY_INVALID_VERSION;

    if (header->identity.osabi != ELF_OSABI_NONE)
        return ELF_VERIFY_INVALID_OSABI;

    return ELF_VERIFY_OK;
}

process_t *elf_create_process(const char *path, process_t *parent, argv_t argv, const stdio_t *ios)
{
    file_t *f = vfs_openat(FD_CWD, path, OPEN_READ | OPEN_EXECUTE);
    if (!f)
    {
        mos_warn("failed to open '%s'", path);
        return NULL;
    }

    io_ref(&f->io);

    const size_t file_size = f->dentry->inode->size;

    const size_t npage_required = ALIGN_UP_TO_PAGE(file_size) / MOS_PAGE_SIZE;
    phyframe_t *const buf_frames = mm_get_free_pages(npage_required);
    char *const buf = (void *) phyframe_va(buf_frames);

    if (!buf_frames)
    {
        mos_warn("failed to allocate %zu pages for '%s'", npage_required, path);
        goto bail_out;
    }

    size_t size = io_read(&f->io, buf, file_size);
    MOS_ASSERT_X(size == file_size, "failed to read entire file '%s'", path);

    const elf_header_t *elf = (elf_header_t *) buf;

    const elf_verify_result verify_result = elf_verify_header(elf);
    if (verify_result != ELF_VERIFY_OK)
    {
        pr_emerg("failed to verify ELF header for '%s', result: %d", path, (int) verify_result);
        goto bail_out;
    }

    if (elf->object_type != ELF_OBJTYPE_EXECUTABLE && elf->object_type != ELF_OBJTYPE_SHARED_OBJECT)
    {
        pr_emerg("'%s' is not an executable", path);
        goto bail_out;
    }

    process_t *proc = process_new(parent, f->dentry->name, ios, (thread_entry_t) elf->entry_point, argv);

    if (!proc)
    {
        mos_warn("failed to create process for '%s'", path);
        goto bail_out;
    }

    for (int i = 0; i < elf->ph.count; i++)
    {
        const elf_program_hdr_t *ph = (elf_program_hdr_t *) (buf + elf->ph_offset + i * elf->ph.entry_size);

        if (unlikely(ph->header_type > _ELF_PT_COUNT))
        {
            mos_warn("invalid program header type 0x%x", ph->header_type);
            continue;
        }

        mos_debug(elf, "program header %d: %c%c%c '%s' at " PTR_FMT, i, //
                  ph->p_flags & ELF_PF_R ? 'r' : '-',                   //
                  ph->p_flags & ELF_PF_W ? 'w' : '-',                   //
                  ph->p_flags & ELF_PF_X ? 'x' : '-',                   //
                  elf_program_header_type_str[ph->header_type],         //
                  ph->vaddr                                             //
        );

        switch (ph->header_type)
        {
            case ELF_PT_NULL: break; // ignore
            case ELF_PT_LOAD:
            {
                //
                // A possible MEMORY layout for a segment:
                //
                //  Page              Page                Page                 Page
                //  Boundary          Boundary            Boundary             Boundary
                //  ||                   ||                   ||                   ||
                //  ||            |Segment Start              ||     Segment End|  ||
                //  ||            |      ||                   ||                |  ||
                //  ||            |      ||                   ||                |  ||
                // -||------------|------||-------------------||-----|----------|--||-----------
                //  || Prev. Seg. | FILE ||         FILE      ||     |   ZERO   |  || Next Seg.
                // -||------------|------||-------------------||-----|----------|--||-----------
                //  ||            |->        size_in_file     ||   <-|          |  ||
                //  ||            |----------->    size_in_mem     <------------|  ||
                //  ||            |-> ph->vaddr               ||                   ||
                //  ||            |-> ph->data_offset         ||                   ||
                //  ||-> A_hole <-|                           ||                   ||
                //  ||----->            A               <-----||----->   B   <-----||
                //  ||-> A_vaddr, start mapping address
                //
                // Meaning:
                //
                // A: An N-page block that doesn't have any zeroed page, i.e., it's fully backed by file
                //      mm_copy_mapping
                //
                // B: A block that has some zeroed pages
                //      memzero, memcpy, mm_copy_mapping
                //
                // C: A block that has all zeroed pages
                //      mm_alloc_zeroed_pages_at
                //
                // In this case, copy_npages = 2 because A can be copied in one go.
                // B is partially copied
                // There's no C because there's no fully zeroed page
                //

                MOS_ASSERT_X(ph->size_in_file <= ph->size_in_mem, "invalid ELF: size in file is larger than size in memory");
                const vm_flags flags = (ph->p_flags & ELF_PF_R ? VM_READ : 0) |  //
                                       (ph->p_flags & ELF_PF_W ? VM_WRITE : 0) | //
                                       (ph->p_flags & ELF_PF_X ? VM_EXEC : 0) |  //
                                       VM_USER;
                const vmap_content_t content = (ph->p_flags & ELF_PF_X ? VMTYPE_CODE : VMTYPE_DATA);

                // determine the number of pages that we can use mm_copy_mapping
                const ptr_t A_vaddr = ALIGN_DOWN_TO_PAGE(ph->vaddr);
                const size_t A_hole = ph->vaddr - A_vaddr;
                const size_t A_size = ALIGN_DOWN_TO_PAGE(A_hole + ph->size_in_file);
                const ptrdiff_t A_file_offset = ph->data_offset - A_hole;
                const size_t A_npages = A_size / MOS_PAGE_SIZE;

                if (A_npages)
                {
                    mos_debug(elf, "copying %zu pages from " PTR_FMT " to address " PTR_FMT, A_npages, (ptr_t) buf + A_file_offset, A_vaddr);
                    const pfn_t A_pfn = phyframe_pfn(buf_frames) + A_file_offset / MOS_PAGE_SIZE;
                    vmblock_t block = mm_map_pages(proc->mm, A_vaddr, A_pfn, A_npages, flags);
                    // vmblock_t block = mm_copy_maps(current_cpu->mm_context, (ptr_t) buf + A_file_offset, proc->mm, A_vaddr, A_npages);
                    // block.flags = flags;
                    // mm_flag_pages(proc->mm, block.vaddr, block.npages, flags);
                    mm_attach_vmap(proc->mm, mm_new_vmap(block, content, (vmap_flags_t){ 0 }));
                }

                // must only has at most one page left
                MOS_ASSERT_X((A_npages + 1) * MOS_PAGE_SIZE >= ph->size_in_file, "invalid ELF: more than one page left");

                // there may be leftover memory (less than a page) that we need to copy
                if (A_npages * MOS_PAGE_SIZE < ph->size_in_file)
                {
                    const ptr_t B_vaddr = A_vaddr + A_size;
                    const size_t B_file_size = A_hole + ph->size_in_file - A_size;
                    const ptr_t B_file_offset = ph->data_offset + ph->size_in_file - B_file_size;

                    // allocate one page
                    const phyframe_t *page = mm_get_free_page();

                    // copy the leftover memory
                    memcpy((void *) phyframe_va(page), buf + B_file_offset, B_file_size);

                    // copy mapping for the leftover memory
                    mos_debug(elf, "elf: leftover %lu bytes from " PTR_FMT " to " PTR_FMT, ph->size_in_file - A_npages * MOS_PAGE_SIZE, phyframe_va(page), B_vaddr);
                    vmblock_t block = mm_map_pages(proc->mm, B_vaddr, phyframe_pfn(page), 1, flags);
                    // block.flags = flags;
                    // mm_flag_pages(proc->mm, block.vaddr, block.npages, flags);
                    mm_attach_vmap(proc->mm, mm_new_vmap(block, content, (vmap_flags_t){ 0 }));
                }

                // allocate the remaining memory, which is not in the file (zeroed)
                const size_t C_npages = ALIGN_UP_TO_PAGE(ph->size_in_mem) / MOS_PAGE_SIZE - ALIGN_UP_TO_PAGE(ph->size_in_file) / MOS_PAGE_SIZE;
                if (C_npages > 0)
                {
                    const ptr_t C_vaddr = ALIGN_UP_TO_PAGE(ph->vaddr + ph->size_in_file);

                    mos_debug(elf, "elf: allocating %zu zero pages at " PTR_FMT, C_npages, C_vaddr);
                    vmblock_t block = mm_alloc_zeroed_pages(proc->mm, C_npages, C_vaddr, VALLOC_DEFAULT, VM_RW);
                    block.flags = flags;
                    mm_attach_vmap(proc->mm, mm_new_vmap(block, content, (vmap_flags_t){ 0 }));
                }

                break;
            }
            case ELF_PT_DYNAMIC:
            case ELF_PT_NOTE:
            case ELF_PT_PHDR:
            case ELF_PT_TLS:
            case ELF_PT_SHLIB:
            {
                if (ph->header_type == ELF_PT_INTERP)
                    pr_info("elf interpreter: %s", buf + ph->data_offset);
                pr_warn("ignoring unsupported program header type %s", elf_program_header_type_str[ph->header_type]);
                break;
            }
            default: pr_warn("unknown program header type 0x%x", ph->header_type); break;
        };
    }

    // unmap the buffer from kernel pages
    mm_unref_pages(buf_frames, npage_required);
    thread_setup_complete(proc->threads[0]);
    io_unref(&f->io); // close the file, we should have the file's refcount == 0 here
    return proc;

bail_out:
    if (buf)
        mm_unref_pages(buf_frames, npage_required);

    if (f)
        io_unref(&f->io); // close the file, we should have the file's refcount == 0 here

    return NULL;
}
