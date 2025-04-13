// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/tasks/process.hpp"
#include "mos/tasks/task_types.hpp"

#include <elf.h>
#include <mos/types.hpp>
#include <mos/vector.hpp>

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

typedef struct
{
    char magic[4];
    u32 bits : 8;
    u32 endianness : 8;
    u32 version : 8;
    u32 osabi : 8;
    u8 abiversion : 8;
    u8 __padding[7];
} elf_identity_t;

MOS_STATIC_ASSERT(sizeof(elf_identity_t) == 16, "elf_identity_t has wrong size");

typedef struct
{
    elf_identity_t identity;
    int object_type : 16;
    int machine_type : 16;

    u32 version;

    ptr_t entry_point;
    size_t ph_offset;
    size_t sh_offset;

    u32 flags;
    u16 header_size;

    struct
    {
        u16 entry_size, count;
    } __packed ph, sh;

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

typedef enum
{
    ELF_PF_X = 1 << 0, // Executable
    ELF_PF_W = 1 << 1, // Writable
    ELF_PF_R = 1 << 2, // Readable
} elf_ph_flags;

typedef struct elf_program_hdr_t
{
    elf_program_header_type header_type;
    elf_ph_flags __p_flags; // Segment independent flags (64-bit only)
    ptr_t data_offset;      // Offset of the segment in the file
    ptr_t vaddr;            // Virtual address of the segment
    ptr_t _reserved;        // reserved
    ptr_t size_in_file;     // Size of the segment in the file (may be 0)
    ptr_t size_in_mem;      // Size of the segment in memory (may be 0)
    ptr_t required_alignment;

    Flags<elf_ph_flags> flags() const
    {
        return __p_flags;
    }
} __packed elf_program_hdr_t;

struct elf_startup_info_t
{
    mos::string invocation;
    mos::vector<Elf64_auxv_t> auxv;
    mos::vector<mos::string> argv;
    mos::vector<mos::string> envp;

    void AddAuxvEntry(u64 type, u64 val)
    {
        auxv.push_back(Elf64_auxv_t{ .a_type = type, .a_un = { .a_val = val } });
    }
};

[[nodiscard]] bool elf_read_and_verify_executable(BasicFile *file, elf_header_t *header);
[[nodiscard]] bool elf_do_fill_process(Process *proc, BasicFile *file, elf_header_t elf, elf_startup_info_t *info);
Process *elf_create_process(const char *path, Process *parent, const char *const argv[], const char *const envp[], const stdio_t *ios);
