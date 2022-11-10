// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/elf/elf.h"

#include "lib/string.h"
#include "mos/filesystem/filesystem.h"
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

process_t *process_create_from_elf(const char *path, process_t *parent, uid_t effective_uid)
{
    file_t *f = vfs_open(path, FILE_OPEN_READ);
    if (!f)
    {
        mos_warn("failed to open '%s'", path);
        goto bail_out_1;
    }

    size_t npage_required = f->io.size / MOS_PAGE_SIZE + 1;
    const vmblock_t buf_block = platform_mm_alloc_pages(current_cpu->pagetable, npage_required, PGALLOC_HINT_USERSPACE, VM_READ | VM_WRITE);
    char *const buf = (char *) buf_block.vaddr;

    size_t size = io_read(&f->io, buf, f->io.size);
    MOS_ASSERT_X(size == f->io.size, "failed to read init");

    elf_header_t *elf = (elf_header_t *) buf;
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

    process_t *proc = process_new(parent, effective_uid, f->fsnode->name, (thread_entry_t) elf->entry_point, NULL);

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

        if (!(ph->header_type & ELF_PH_T_LOAD))
            continue; // skip non-loadable segments

        vm_flags map_flags = (                          //
            (ph->p_flags & ELF_PH_F_R ? VM_READ : 0) |  //
            (ph->p_flags & ELF_PH_F_W ? VM_WRITE : 0) | //
            (ph->p_flags & ELF_PH_F_X ? VM_EXEC : 0) |  //
            VM_USER                                     //
        );

        // rwx
        if (map_flags & VM_READ && map_flags & VM_WRITE && map_flags & VM_EXEC)
            mos_warn("segment is writable, readable and executable");

        vmblock_t block = platform_mm_copy_maps(                 //
            current_cpu->pagetable,                              //
            buf_block.vaddr + ph->data_offset,                   //
            proc->pagetable,                                     //
            ALIGN_DOWN_TO_PAGE(ph->vaddr),                       //
            ALIGN_UP_TO_PAGE(ph->segsize_in_mem) / MOS_PAGE_SIZE //
        );

        platform_mm_flag_pages(proc->pagetable, block.vaddr, block.npages, map_flags);
        block.flags = map_flags;

        process_attach_mmap(proc, block, (ph->p_flags & ELF_PH_F_X) ? VMTYPE_APPCODE : VMTYPE_APPDATA, false);
    }

#if MOS_DEBUG
    process_dump_mmaps(proc);
    const char *const strtab = buf + ((elf_section_hdr_t *) (buf + elf->sh_offset + elf->sh_strtab_index * elf->sh.entry_size))->sh_offset;
    for (int i = 0; i < elf->sh.count; i++)
    {
        elf_section_hdr_t *sh = (elf_section_hdr_t *) (buf + elf->sh_offset + i * elf->sh.entry_size);
        pr_info2("ELF section %2d: %s", i, strtab + sh->name_index);
    }
#endif

    process_attach_fd(proc, &f->io);

    // unmap the buffer from kernel pages
    platform_mm_unmap_pages(current_cpu->pagetable, buf_block.vaddr, buf_block.npages);

    return proc;

bail_out:
    if (buf)
        platform_mm_free_pages(current_cpu->pagetable, buf_block.vaddr, buf_block.npages);
bail_out_1:
    if (f)
        io_close(&f->io);
    return NULL;
}
