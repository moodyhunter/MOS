// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/tasks/task_types.h"
#include "mos/types.h"

typedef enum
{
    ELF_BITS_INVALID = 0,
    ELF_BITS_32 = 1,
    ELF_BITS_64 = 2,
} elf_bits;

#if MOS_BITS == 32
#define ELF_BITS_MOS_DEFAULT ELF_BITS_32
#elif MOS_BITS == 64
#define ELF_BITS_MOS_DEFAULT ELF_BITS_64
#else
#error "Unsupported MOS_BITS"
#endif

typedef enum
{
    ELF_ENDIANNESS_INVALID = 0,
    ELF_ENDIANNESS_LSB = 1,
    ELF_ENDIANNESS_MSB = 2,
} elf_endianness;

#if MOS_LITTLE_ENDIAN
#define ELF_ENDIANNESS_MOS_DEFAULT ELF_ENDIANNESS_LSB
#else
#define ELF_ENDIANNESS_MOS_DEFAULT ELF_ENDIANNESS_MSB
#endif

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

MOS_STATIC_ASSERT(sizeof(elf_identity_t) == 16, "elf_identity_t has wrong size");

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

#if defined(__i386__)
#define ELF_MACHINE_MOS_DEFAULT ELF_MACHINE_X86
#elif defined(__x86_64__)
#define ELF_MACHINE_MOS_DEFAULT ELF_MACHINE_X86_64
#else
#error "Unsupported architecture"
#endif

typedef struct
{
    elf_identity_t identity;
    elf_object_type object_type : 16;
    elf_machine_type machine_type : 16;

    u32 version;

    uintptr_t entry_point;
    size_t ph_offset;
    size_t sh_offset;

    u32 flags;
    u16 header_size;

    struct
    {
        u16 entry_size, count;
    } __packed ph;

    struct
    {
        u16 entry_size, count;
    } __packed sh;

    u16 sh_strtab_index;
} __packed elf_header_t;

typedef enum
{
    ELF_PT_NULL = 0,    // Unused entry
    ELF_PT_LOAD = 1,    // Loadable segment
    ELF_PT_DYNAMIC = 2, // Dynamic linking information
    ELF_PT_INTERP = 3,  // Interpreter information
    ELF_PT_NOTE = 4,    // Auxiliary information
    ELF_PT_SHLIB = 5,   // reserved
    ELF_PT_PHDR = 6,    // Segment containing program header table
    ELF_PT_TLS = 7,     // Thread-local storage segment

    _ELF_PT_COUNT,

    ELF_PT_OS_LOW = 0x60000000,       // reserved
    ELF_PT_OS_HIGH = 0x6fffffff,      // reserved
    ELF_PT_PROCESSOR_LO = 0x70000000, // reserved
    ELF_PT_PROCESSOR_HI = 0x7fffffff, // reserved
} elf_program_header_type;

extern const char *elf_program_header_type_str[_ELF_PT_COUNT];

typedef enum
{
    ELF_PF_X = 1 << 0, // Executable
    ELF_PF_W = 1 << 1, // Writable
    ELF_PF_R = 1 << 2, // Readable
} elf_ph_flags;

typedef struct
{
    elf_program_header_type header_type;
#if MOS_BITS == 64
    elf_program_header_flags p_flags; // Segment independent flags (64-bit only)
#endif
    uintptr_t data_offset;  // Offset of the segment in the file
    uintptr_t vaddr;        // Virtual address of the segment
    uintptr_t _reserved;    // reserved
    uintptr_t size_in_file; // Size of the segment in the file (may be 0)
    uintptr_t size_in_mem;  // Size of the segment in memory (may be 0)
#if MOS_BITS == 32
    elf_ph_flags p_flags; // Segment independent flags (32-bit only)
#endif
    uintptr_t required_alignment;
} __packed elf_program_hdr_t;

typedef enum
{
    ELF_ST_NULL = 0,           // Unused entry
    ELF_ST_PROGBITS = 1,       // Program data
    ELF_ST_SYMTAB = 2,         // Symbol table
    ELF_ST_STRTAB = 3,         // String table
    ELF_ST_RELA = 4,           // Relocation entries with addends
    ELF_ST_HASH = 5,           // Symbol hash table
    ELF_ST_DYNAMIC = 6,        // Dynamic linking information
    ELF_ST_NOTE = 7,           // Auxiliary information
    ELF_ST_NOBITS = 8,         // Data
    ELF_ST_REL = 9,            // Relocation entries without addends`
    ELF_ST_SHLIB = 10,         // Reserved
    ELF_ST_DYNSYM = 11,        // Dynamic linker symbol table
    ELF_ST_INIT_ARRAY = 14,    // Array of constructors
    ELF_ST_FINI_ARRAY = 15,    // Array of destructors
    ELF_ST_PREINIT_ARRAY = 16, // Array of pre-constructors
    ELF_ST_GROUP = 17,         // Section group
    ELF_ST_SYMTAB_SHNDX = 18,  // Extended section indeces
    ELF_ST_NUM = 19,           // Number of defined types
    ELF_ST_LOOS = 0x60000000,  // Start of OS-specific
} elf_section_header_type;

typedef enum
{
    ELF_SF_WRITE = 1,                // Writable
    ELF_SF_ALLOC = 2,                // Occupies memory during execution
    ELF_SF_EXECINSTR = 4,            // Executable
    ELF_SF_MERGE = 0x10,             // Might be merged
    ELF_SF_STRINGS = 0x20,           // Contains nul-terminated strings
    ELF_SF_INFO_LINK = 0x40,         // `sh_info' contains SHT index
    ELF_SF_LINK_ORDER = 0x80,        // Preserve order after combining
    ELF_SF_OS_NONCONFORMING = 0x100, // Non-standard OS specific
    ELF_SF_GROUP = 0x200,            // Section is member of a group
    ELF_SF_TLS = 0x400,              // Section hold thread-local data
} elf_section_flags;

typedef struct
{
    u32 name_index; // Section name (string table (.shstrtab) index)
    elf_section_header_type header_type;
#if MOS_BITS == 64
    u64 attributes; // sizeof(long)
#elif MOS_BITS == 32
    elf_section_flags attributes;
#else
#error "Unsupported bitness"
#endif
    uintptr_t sh_addr;   // Virtual address of the section in memory, if loaded
    uintptr_t sh_offset; // Offset of the section in the file
    size_t sh_size;      // Size of the section in bytes
    u32 sh_link;
    u32 sh_info;
    uintptr_t sh_addralign;
    size_t sh_entsize;
} __packed elf_section_hdr_t;

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

elf_verify_result elf_verify_header(const elf_header_t *header);
process_t *elf_create_process(const char *path, process_t *parent, terminal_t *term, argv_t argv);
