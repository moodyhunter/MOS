// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/elf/elf.h"

#include "lib/string.h"

#ifdef MOS_32BITS
static_assert(sizeof(elf_header_t) == 0x34, "elf_header has wrong size");
static_assert(sizeof(elf_program_header_t) == 0x20, "elf_program_header has wrong size");
static_assert(sizeof(elf_section_header_t) == 0x28, "elf_section_header has wrong size");
#else
static_assert(sizeof(elf_header_t) == 0x40, "elf_header has wrong size");
static_assert(sizeof(elf_program_header_t) == 0x30, "elf_program_header has wrong size");
static_assert(sizeof(elf_section_header_t) == 0x38, "elf_section_header has wrong size");
#endif

elf_verify_result mos_elf_verify_header(elf_header_t *header)
{
    if (header->identity.magic[0] != '\x7f')
        return ELF_VERIFY_INVALID_MAGIC;

    if (strncmp(&header->identity.magic[1], "ELF", 3) != 0)
        return ELF_VERIFY_INVALID_MAGIC_ELF;

    if (header->identity.bits != ELF_BITS_DEFAULT)
        return ELF_VERIFY_INVALID_BITS;

    if (header->identity.endianness != ELF_ENDIANNESS_DEFAULT)
        return ELF_VERIFY_INVALID_ENDIAN;

    if (header->identity.version != ELF_VERSION_CURRENT)
        return ELF_VERIFY_INVALID_VERSION;

    if (header->identity.osabi != ELF_OSABI_MOS)
        return ELF_VERIFY_INVALID_OSABI;

    return true;
}
