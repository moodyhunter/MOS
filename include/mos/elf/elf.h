// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef enum
{
    ELF_BITS_INVALID = 0,
    ELF_BITS_32 = 1,
    ELF_BITS_64 = 2,
#ifdef MOS_32BITS
    ELF_BITS_DEFAULT = ELF_BITS_32,
#else
    ELF_BITS_DEFAULT = ELF_BITS_64,
#endif
} elf_bits;

typedef enum
{
    ELF_ENDIANNESS_INVALID = 0,
    ELF_ENDIANNESS_LSB = 1,
    ELF_ENDIANNESS_MSB = 2,
#ifdef MOS_LITTLE_ENDIAN
    ELF_ENDIANNESS_DEFAULT = ELF_ENDIANNESS_LSB,
#else
    ELF_ENDIANNESS_DEFAULT = ELF_ENDIANNESS_MSB,
#endif
} elf_endianness;

typedef enum
{
    ELF_VERSION_NONE = 0,
    ELF_VERSION_CURRENT = 1,
} elf_version_type;

typedef enum
{
    ELF_OSABI_NONE = 0,
    ELF_OSABI_LINUX = 3,
    ELF_OSABI_HURD = 4,
    ELF_OSABI_SOLARIS = 6,
    ELF_OSABI_FREEBSD = 9,
    ELF_OSABI_ARM_AEABI = 64,
    ELF_OSABI_ARM = 97,
    ELF_OSABI_MOS = 254, // ! Long live the MOS!
    ELF_OSABI_STANDALONE = 255,
} elf_osabi_type;

typedef struct
{
    char magic[4];
    elf_bits bits : 8;
    elf_endianness endianness : 8;
    elf_version_type version : 8;
    elf_osabi_type osabi : 8;
    u8 abiversion : 8;
    u8 __padding[7];
} elf_identity_t;

static_assert(sizeof(elf_identity_t) == 16, "elf_identity_t has wrong size");

typedef enum
{
    ELF_OBJTYPE_NONE = 0,
    ELF_OBJTYPE_RELOCATABLE = 1,
    ELF_OBJTYPE_EXECUTABLE = 2,
    ELF_OBJTYPE_SHARED_OBJECT = 3,
    ELF_OBJTYPE_CORE = 4,
    ELF_OBJTYPE_PROCESSOR_SPECIFIC_LO = 0xff00,
    ELF_OBJTYPE_PROCESSOR_SPECIFIC_HI = 0xffff,
} elf_object_type;

typedef enum
{
    ELF_MACHINE_NONE = 0,
    ELF_MACHINE_X86 = 0x03,
    ELF_MACHINE_MIPS = 0x08,
    ELF_MACHINE_ARM = 0x28,
    ELF_MACHINE_X86_64 = 0x3e,
    ELF_MACHINE_AARCH64 = 0xb7,
    ELF_MACHINE_RISCV = 0xf3,
} elf_machine_type;

typedef struct
{
    elf_identity_t identity;
    elf_object_type object_type : 16;
    elf_machine_type machine_type : 16;

    u32 version;

    uintptr_t entry_point;
    uintptr_t program_header_offset;
    uintptr_t section_header_offset;

    u32 flags;
    u16 header_size;

    struct
    {
        u16 entry_size, count;
    } __packed program_header;

    struct
    {
        u16 entry_size, count;
    } __packed section_header;

    u16 sh_string_table_index;
} __packed elf_header_t;

typedef enum
{
    ELF_PH_T_NULL = 0,    // Unused entry
    ELF_PH_T_LOAD = 1,    // Loadable segment
    ELF_PH_T_DYNAMIC = 2, // Dynamic linking information
    ELF_PH_T_INTERP = 3,  // Interpreter information
    ELF_PH_T_NOTE = 4,    // Auxiliary information
    ELF_PH_T_SHLIB = 5,   // reserved
    ELF_PH_T_PHDR = 6,    // Segment containing program header table
    ELF_PH_T_TLS = 7,     // Thread-local storage segment

    ELF_PH_T_OS_LOW = 0x60000000,       // reserved
    ELF_PH_T_OS_HIGH = 0x6fffffff,      // reserved
    ELF_PH_T_PROCESSOR_LO = 0x70000000, // reserved
    ELF_PH_T_PROCESSOR_HI = 0x7fffffff, // reserved
} elf_program_header_type;

typedef struct
{
    elf_program_header_type p_type;
#ifdef MOS_64BITS
    u32 p_flags; // Segment independent flags (64-bit only)
#endif
    uintptr_t p_offset; // Offset of the segment in the file
    uintptr_t p_vaddr;  // Virtual address of the segment
    uintptr_t p_paddr;  // Physical address of the segment (reserved)
    uintptr_t p_filesz; // Size of the segment in the file (may be 0)
    uintptr_t p_memsz;  // Size of the segment in memory (may be 0)
#ifdef MOS_32BITS
    u32 p_flags; // Segment independent flags (32-bit only)
#endif
    uintptr_t p_align;
} __packed elf_program_header_t;

typedef enum
{
    ELF_SH_T_NULL = 0,           // Unused entry
    ELF_SH_T_PROGBITS = 1,       // Program data
    ELF_SH_T_SYMTAB = 2,         // Symbol table
    ELF_SH_T_STRTAB = 3,         // String table
    ELF_SH_T_RELA = 4,           // Relocation entries with addends
    ELF_SH_T_HASH = 5,           // Symbol hash table
    ELF_SH_T_DYNAMIC = 6,        // Dynamic linking information
    ELF_SH_T_NOTE = 7,           // Auxiliary information
    ELF_SH_T_NOBITS = 8,         // Data
    ELF_SH_T_REL = 9,            // Relocation entries without addends`
    ELF_SH_T_SHLIB = 10,         // Reserved
    ELF_SH_T_DYNSYM = 11,        // Dynamic linker symbol table
    ELF_SH_T_INIT_ARRAY = 14,    // Array of constructors
    ELF_SH_T_FINI_ARRAY = 15,    // Array of destructors
    ELF_SH_T_PREINIT_ARRAY = 16, // Array of pre-constructors
    ELF_SH_T_GROUP = 17,         // Section group
    ELF_SH_T_SYMTAB_SHNDX = 18,  // Extended section indeces
    ELF_SH_T_NUM = 19,           // Number of defined types
    ELF_SH_T_LOOS = 0x60000000,  // Start of OS-specific
} elf_section_header_type;

typedef enum
{
    ELF_SH_ATTR_WRITE = 1,                // Writable
    ELF_SH_ATTR_ALLOC = 2,                // Occupies memory during execution
    ELF_SH_ATTR_EXECINSTR = 4,            // Executable
    ELF_SH_ATTR_MERGE = 0x10,             // Might be merged
    ELF_SH_ATTR_STRINGS = 0x20,           // Contains nul-terminated strings
    ELF_SH_ATTR_INFO_LINK = 0x40,         // `sh_info' contains SHT index
    ELF_SH_ATTR_LINK_ORDER = 0x80,        // Preserve order after combining
    ELF_SH_ATTR_OS_NONCONFORMING = 0x100, // Non-standard OS specific
    ELF_SH_ATTR_GROUP = 0x200,            // Section is member of a group
    ELF_SH_ATTR_TLS = 0x400,              // Section hold thread-local data
} elf_section_attribute;

typedef struct
{
    u32 name_index; // Section name (string table (.shstrtab) index)
    elf_section_header_type header_type;
#ifdef MOS_64BITS
    u64 attributes; // sizeof(long)
#else
    elf_section_attribute attributes;
#endif
    uintptr_t sh_addr;   // Virtual address of the section in memory, if loaded
    uintptr_t sh_offset; // Offset of the section in the file
    size_t sh_size;      // Size of the section in bytes
    u32 sh_link;
    u32 sh_info;
    uintptr_t sh_addralign;
    size_t sh_entsize;
} __packed elf_section_header_t;

typedef enum
{
    ELF_VERIFY_OK = 0,
    ELF_VERIFY_INVALID_MAGIC,
    ELF_VERIFY_INVALID_MAGIC_ELF,
    ELF_VERIFY_INVALID_BITS,
    ELF_VERIFY_INVALID_ENDIAN,
    ELF_VERIFY_INVALID_VERSION,
    ELF_VERIFY_INVALID_OSABI,
} elf_verify_result;

elf_verify_result mos_elf_verify_header(elf_header_t *header);
