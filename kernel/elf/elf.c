// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/elf/elf.h"

#include "mos/mm/cow.h"
#include "mos/mm/mm.h"
#include "mos/mm/mmap.h"
#include "mos/mm/physical/pmm.h"
#include "mos/tasks/process.h"
#include "mos/tasks/thread.h"

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

bool elf_read_and_verify_executable(file_t *file, elf_header_t *header)
{
    elf_read_file(file, header, 0, sizeof(elf_header_t));
    const bool valid = elf_verify_header(header);
    if (!valid)
        return false;

    if (header->object_type != ELF_OBJTYPE_EXECUTABLE && header->object_type != ELF_OBJTYPE_SHARED_OBJECT)
        return false;

    return true;
}

process_t *elf_create_process(file_t *file, process_t *parent, argv_t argv, const stdio_t *ios)
{
    elf_header_t elf;
    if (!elf_read_and_verify_executable(file, &elf))
    {
        pr_emerg("failed to verify ELF header for '%s'", dentry_name(file->dentry));
        return NULL;
    }

    process_t *proc = process_new(parent, file->dentry->name, ios, argv);
    if (!proc)
    {
        mos_warn("failed to create process for '%s'", dentry_name(file->dentry));
        return NULL;
    }

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

    thread_setup_complete(proc->main_thread, (thread_entry_t) elf.entry_point, NULL);
    mm_switch_context(prev_mm);
    return proc;
}
