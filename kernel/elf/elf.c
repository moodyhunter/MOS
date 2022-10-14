// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/elf/elf.h"

#include "lib/string.h"

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
