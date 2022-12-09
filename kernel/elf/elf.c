// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/elf/elf.h"

#include "lib/string.h"
#include "mos/filesystem/filesystem.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/memops.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/types.h"

static_assert(sizeof(elf_header_t) == (MOS_32BITS ? 0x34 : 0x40), "elf_header has wrong size");
static_assert(sizeof(elf_program_hdr_t) == (MOS_32BITS ? 0x20 : 0x38), "elf_program_header has wrong size");
static_assert(sizeof(elf_section_hdr_t) == (MOS_32BITS ? 0x28 : 0x40), "elf_section_header has wrong size");

elf_verify_result elf_verify_header(elf_header_t *header)
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

process_t *elf_create_process(const char *path, process_t *parent, terminal_t *term, uid_t effective_uid)
{
    file_t *f = vfs_open(path, FILE_OPEN_READ);
    if (!f)
    {
        mos_warn("failed to open '%s'", path);
        goto bail_out_1;
    }

    size_t npage_required = ALIGN_UP_TO_PAGE(f->io.size) / MOS_PAGE_SIZE;
    const vmblock_t buf_block = platform_mm_alloc_pages(current_cpu->pagetable, npage_required, PGALLOC_HINT_KHEAP, VM_READ | VM_WRITE);
    char *const buf = (char *) buf_block.vaddr;

    size_t size = io_read(&f->io, buf, f->io.size);
    MOS_ASSERT_X(size == f->io.size, "failed to read entire file '%s'", path);

    elf_header_t *elf = (elf_header_t *) buf;
    elf_program_hdr_t **ph_list = kcalloc(sizeof(elf_program_hdr_t *), elf->ph.count);

    if (elf->object_type != ELF_OBJTYPE_EXECUTABLE)
    {
        pr_emerg("'%s' is not an executable", path);
        goto bail_out;
    }

    elf_verify_result verify_result = elf_verify_header(elf);
    if (verify_result != ELF_VERIFY_OK)
    {
        pr_emerg("failed to verify ELF header for '%s', result: %d", path, (int) verify_result);
        goto bail_out;
    }

    process_t *proc = process_new(parent, effective_uid, f->fsnode->name, term, (thread_entry_t) elf->entry_point, NULL);

    for (int i = 0; i < elf->ph.count; i++)
    {
        elf_program_hdr_t *ph = (elf_program_hdr_t *) (buf + elf->ph_offset + i * elf->ph.entry_size);
        mos_debug("program header %d: %c%c%c%s at " PTR_FMT, i,
                  (elf_program_header_flags) ph->p_flags & ELF_PH_F_R ? 'r' : '-', //
                  (elf_program_header_flags) ph->p_flags & ELF_PH_F_W ? 'w' : '-', //
                  (elf_program_header_flags) ph->p_flags & ELF_PH_F_X ? 'x' : '-', //
                  ph->header_type == ELF_PH_T_LOAD ? " (load)" : "",               //
                  ph->vaddr                                                        //
        );

        // skip non-loadable segments
        if (!(ph->header_type & ELF_PH_T_LOAD))
            continue;

        ph_list[i] = ph;
    }

    // previous_sh is used to deterimine the ending address of the previous section
    //! assuming that the sections are sorted by their address (which should be?)
    elf_section_hdr_t *previouse_sh = NULL;

    const char *const strtab = buf + ((elf_section_hdr_t *) (buf + elf->sh_offset + elf->sh_strtab_index * elf->sh.entry_size))->sh_offset;
    for (int sh_i = 0; sh_i < elf->sh.count; sh_i++)
    {
        elf_section_hdr_t *sh = (elf_section_hdr_t *) (buf + elf->sh_offset + sh_i * elf->sh.entry_size);
        const char *const name = &strtab[sh->name_index];
        mos_debug("elf section %2d: %s", sh_i, name);

        if (sh->sh_size == 0)
            continue; // skip empty sections

        const uintptr_t section_inmem_addr = sh->sh_addr;
        bool is_loadable = false;

        vm_flags map_flags = VM_USER;
        for (int ph_i = 0; ph_i < elf->ph.count; ph_i++)
        {
            elf_program_hdr_t *ph = ph_list[ph_i];
            if (!ph)
                continue;

            if (section_inmem_addr >= ph->vaddr && section_inmem_addr < ph->vaddr + ph->segsize_in_mem)
            {
                MOS_ASSERT_X(!is_loadable, "section %d is loadable in multiple program headers", sh_i);
                mos_debug("section %d (%s), header %d, file offset " PTR_FMT " (%zu bytes) -> mem " PTR_FMT, sh_i, name, ph_i, sh->sh_offset, sh->sh_size,
                          section_inmem_addr);
                is_loadable = true;
                map_flags |= ((ph->p_flags & ELF_PH_F_R ? VM_READ : 0) |  //
                              (ph->p_flags & ELF_PH_F_W ? VM_WRITE : 0) | //
                              (ph->p_flags & ELF_PH_F_X ? VM_EXEC : 0));
            }
        }

        if (!is_loadable)
        {
            mos_debug("section %d is not loadable", sh_i);
            continue;
        }

        if (unlikely(map_flags & VM_READ && map_flags & VM_WRITE && map_flags & VM_EXEC))
            mos_warn("section %d (%s) is both readable, writable and executable", sh_i, name);

        const size_t section_inmem_pages = ALIGN_UP_TO_PAGE(sh->sh_size) / MOS_PAGE_SIZE;

        // find if there are any pages that are already mapped because sections may not be page-aligned
        size_t mapped_pages_n = 0;
        for (mapped_pages_n = 0; mapped_pages_n < section_inmem_pages; mapped_pages_n++)
        {
            uintptr_t page_addr = ALIGN_DOWN_TO_PAGE(section_inmem_addr) + mapped_pages_n * MOS_PAGE_SIZE;
            if (unlikely(platform_mm_get_is_mapped(proc->pagetable, page_addr)))
                continue;
            break;
        }

        // if mapped_pages_n > 1, then this section must have [at least one page] that is overlapping with another section
        MOS_ASSERT_X(mapped_pages_n == 0 || mapped_pages_n == 1, "a section is partially mapped in multiple pages, this is impossible");

        const size_t pages_to_map = section_inmem_pages - mapped_pages_n;
        const uintptr_t first_unmapped_addr = ALIGN_DOWN_TO_PAGE(section_inmem_addr) + mapped_pages_n * MOS_PAGE_SIZE;

        if (mapped_pages_n == 0)
        {
            mos_debug("section %d is not previously mapped", sh_i);
        }
        else
        {
            // this section is previously partially (or fully) mapped, check if it's a NOBITS section and possibly memzero on its
            // previously mapped pages
            if (sh->header_type == ELF_SH_T_NOBITS && mapped_pages_n)
            {
                mos_debug("section %d is NOBITS, zeroing previously mapped area", sh_i);
                MOS_ASSERT(previouse_sh); // It's just impossible to have a previously mapped section if this is the first section

                // Memory layout:
                //                                                  'A'
                // ...... ->|<- ....... previous page ............ ->|<- this page ...........
                // .... previous section ->| gap |<- this section ...........................
                //                               |<-  [zero_size]  ->|<- [first_unmapped_addr]
                //                              'B'

                const uintptr_t A = ALIGN_UP_TO_PAGE((uintptr_t) buf + previouse_sh->sh_offset + previouse_sh->sh_size);
                const uintptr_t zero_size = ALIGN_UP_TO_PAGE(sh->sh_addr) - sh->sh_addr;
                const uintptr_t B = A - zero_size;
                memzero((void *) B, zero_size);
            }

            if (mapped_pages_n == section_inmem_pages)
            {
                mos_debug("section %d is already fully mapped", sh_i);
                continue;
            }
            else
            {
                mos_debug("section %d is partially mapped (%zu pages), mapping the rest (%zu pages)", sh_i, mapped_pages_n, pages_to_map);
            }
        }

        previouse_sh = sh;

        if (mapped_pages_n == section_inmem_pages)
            continue; // all pages are already mapped

        if (sh->header_type == ELF_SH_T_NOBITS)
        {
            vmblock_t zero_block = mm_alloc_zeroed_pages_at(proc->pagetable, first_unmapped_addr, pages_to_map, map_flags);
            process_attach_mmap(proc, zero_block, VMTYPE_APPDATA_ZERO, MMAP_COW); // zero-fill-on-demand is CoW
        }
        else
        {
            uintptr_t file_offset = ALIGN_DOWN_TO_PAGE((uintptr_t) buf + sh->sh_offset) + mapped_pages_n * MOS_PAGE_SIZE;
            vmblock_t block = platform_mm_copy_maps(current_cpu->pagetable, file_offset, proc->pagetable, first_unmapped_addr, pages_to_map);
            platform_mm_flag_pages(proc->pagetable, block.vaddr, block.npages, map_flags);
            block.flags = map_flags;
            process_attach_mmap(proc, block, map_flags & VM_EXEC ? VMTYPE_APPCODE : VMTYPE_APPDATA, MMAP_DEFAULT);
        }
    }

    process_attach_ref_fd(proc, &f->io); // do we need this?

    // unmap the buffer from kernel pages
    platform_mm_unmap_pages(current_cpu->pagetable, buf_block.vaddr, buf_block.npages);

    return proc;

bail_out:
    if (ph_list)
        kfree(ph_list);
    if (buf)
        platform_mm_free_pages(current_cpu->pagetable, buf_block.vaddr, buf_block.npages);
bail_out_1:
    if (f)
        io_unref(&f->io); // close the file, we should have the file's refcount == 0 here

    return NULL;
}
