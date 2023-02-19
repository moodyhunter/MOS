/**
 * @file elf.c
 * @author ajxs
 * @date Aug 2019
 * @brief Functionality for working with ELF executable files.
 * Contains functionality to assist in loading and validating ELF executable
 * files. This functionality is essential to the ELF executable loader.
 */

#include "elf.h"

#include "common.h"

static const CHAR16 *const osabi[] = {
    L"UNIX System V",
    L"HP-UX",
    L"NetBSD",
    L"Linux",
    L"GNU Hurd",
    L"Solaris",
    L"AIX",
    L"IRIX",
    L"FreeBSD",
    L"Tru64",
    L"Novell Modesto",
    L"OpenBSD",
    L"OpenVMS",
    L"HP Non-Stop Kernel",
    L"Amiga Research OS",
    L"The AROS Research OS",
    L"Fenix OS",
    L"CloudABI",
    L"Stratus Technologies OpenVOS",
};

static const CHAR16 *const machine_type[] = {
    [0x00] = L"None", [0x02] = L"SPARC",  [0x03] = L"x86",   [0x08] = L"MIPS",   [0x14] = L"PowerPC", [0x16] = L"S390",
    [0x28] = L"ARM",  [0x2A] = L"SuperH", [0x32] = L"IA-64", [0x3E] = L"x86-64", [0xB7] = L"AArch64", [0xF3] = L"RISC-V",
};

static const CHAR16 *const file_type[] = { L"None", L"Relocatable", L"Executable", L"Shared Object", L"Core" };

VOID print_elf_file_info(IN VOID *const header_ptr, IN VOID *const program_headers_ptr)
{
    /**
     * The header pointer cast to a 32bit ELF Header so that we can read the
     * header and determine the file type. Then cast to a 64bit header for
     * specific fields if necessary.
     */
    Elf32_Ehdr *header = (Elf32_Ehdr *) header_ptr;
    Elf64_Ehdr *header64 = (Elf64_Ehdr *) header_ptr;

    Log("<ELF Header Info>");
    Log("       Class: '%s'", header->e_ident[EI_CLASS] == ELF_FILE_CLASS_32 ? L"32-bit" : L"64-bit");
    Log("  Endianness: '%s'", header->e_ident[EI_DATA] == 1 ? L"Little-Endian" : L"Big-Endian");
    Log("     Version: '0x%x'", header->e_ident[EI_VERSION]);
    Log("      OS ABI: '%s'", osabi[header->e_ident[EI_OSABI]]);
    Log("   File Type: '%s'", file_type[header->e_type]);
    Log("     Machine: '%s'", machine_type[header->e_machine]);

    if (header->e_ident[EI_CLASS] == ELF_FILE_CLASS_32)
    {
        Log("  Entry point:              0x%lx", header->e_entry);
        Log("  Program header offset:    0x%lx", header->e_phoff);
        Log("  Section header offset:    0x%lx", header->e_shoff);
        Log("  Program header count:     %u", header->e_phnum);
        Log("  Section header count:     %u", header->e_shnum);

        Elf32_Phdr *program_headers = program_headers_ptr;

        Log("\nProgram Headers:");
        UINTN p = 0;
        for (p = 0; p < header->e_phnum; p++)
        {
            Log("[%u]:", p);
            Log("  p_type:      0x%lx", program_headers[p].p_type);
            Log("  p_offset:    0x%lx", program_headers[p].p_offset);
            Log("  p_vaddr:     0x%lx", program_headers[p].p_vaddr);
            Log("  p_paddr:     0x%lx", program_headers[p].p_paddr);
            Log("  p_filesz:    0x%lx", program_headers[p].p_filesz);
            Log("  p_memsz:     0x%lx", program_headers[p].p_memsz);
            Log("  p_flags:     0x%lx", program_headers[p].p_flags);
            Log("  p_align:     0x%lx", program_headers[p].p_align);
        }
    }
    else if (header->e_ident[EI_CLASS] == ELF_FILE_CLASS_64)
    {
        Log("  Entry point:              0x%llx", header64->e_entry);
        Log("  Program header offset:    0x%llx", header64->e_phoff);
        Log("  Section header offset:    0x%llx", header64->e_shoff);
        Log("  Program header count:     %u", header64->e_phnum);
        Log("  Section header count:     %u", header64->e_shnum);

        Elf64_Phdr *program_headers = program_headers_ptr;

        Log("\nDebug: Program Headers:");
        UINTN p = 0;
        for (p = 0; p < header64->e_phnum; p++)
        {
            Log("[%u]:", p);
            Log("  p_type:      0x%lx", program_headers[p].p_type);
            Log("  p_flags:     0x%lx", program_headers[p].p_flags);
            Log("  p_offset:    0x%llx", program_headers[p].p_offset);
            Log("  p_vaddr:     0x%llx", program_headers[p].p_vaddr);
            Log("  p_paddr:     0x%llx", program_headers[p].p_paddr);
            Log("  p_filesz:    0x%llx", program_headers[p].p_filesz);
            Log("  p_memsz:     0x%llx", program_headers[p].p_memsz);
            Log("  p_align:     0x%llx", program_headers[p].p_align);
        }
    }
}

EFI_STATUS read_elf_file(IN EFI_FILE *const kernel_img_file, IN Elf_File_Class const file_class, OUT VOID **kernel_header_buffer,
                         OUT VOID **kernel_program_headers_buffer)
{
    UINTN buffer_read_size = 0;
    UINTN program_headers_offset = 0;

    // Reset to start of file.
#if MOS_LOADER_DEBUG
    Log("Debug: Setting file pointer to read executable header");
#endif

    EFI_STATUS status = uefi_call_wrapper(kernel_img_file->SetPosition, 2, kernel_img_file, 0);
    if (EFI_ERROR(status))
    {
        Log("Error: Error setting file pointer position: %r", status);

        return status;
    }

    if (file_class == ELF_FILE_CLASS_32)
        buffer_read_size = sizeof(Elf32_Ehdr);
    else if (file_class == ELF_FILE_CLASS_64)
        buffer_read_size = sizeof(Elf64_Ehdr);
    else
    {
        Log("Error: Invalid file class", status);
        return EFI_INVALID_PARAMETER;
    }

#if MOS_LOADER_DEBUG
    Log("Debug: Allocating '0x%lx' for kernel executable header buffer", buffer_read_size);
#endif

    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, buffer_read_size, kernel_header_buffer);
    if (EFI_ERROR(status))
    {
        Log("Error: Error allocating kernel header buffer: %r", status);
        return status;
    }

#if MOS_LOADER_DEBUG
    Log("Debug: Reading kernel executable header");
#endif

    status = uefi_call_wrapper(kernel_img_file->Read, 3, kernel_img_file, &buffer_read_size, *kernel_header_buffer);
    if (EFI_ERROR(status))
    {
        Log("Error: Error reading kernel header: %r", status);
        return status;
    }

    if (file_class == ELF_FILE_CLASS_32)
    {
        program_headers_offset = ((Elf32_Ehdr *) *kernel_header_buffer)->e_phoff;
        buffer_read_size = sizeof(Elf32_Phdr) * ((Elf32_Ehdr *) *kernel_header_buffer)->e_phnum;
    }
    else if (file_class == ELF_FILE_CLASS_64)
    {
        program_headers_offset = ((Elf64_Ehdr *) *kernel_header_buffer)->e_phoff;
        buffer_read_size = sizeof(Elf64_Phdr) * ((Elf64_Ehdr *) *kernel_header_buffer)->e_phnum;
    }

#if MOS_LOADER_DEBUG
    Log("Debug: Setting file offset to '0x%lx' to read program headers", program_headers_offset);
#endif

    // Read program headers.
    status = uefi_call_wrapper(kernel_img_file->SetPosition, 2, kernel_img_file, program_headers_offset);
    if (EFI_ERROR(status))
    {
        Log("Error: Error setting file pointer position: %r", status);
        return status;
    }

// Allocate memory for program headers.
#if MOS_LOADER_DEBUG
    Log("Debug: Allocating '0x%lx' for program headers buffer", buffer_read_size);
#endif

    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, buffer_read_size, kernel_program_headers_buffer);
    if (EFI_ERROR(status))
    {
        Log("Error: Error allocating kernel program header buffer: %r", status);
        return status;
    }

#if MOS_LOADER_DEBUG
    Log("Debug: Reading program headers");
#endif

    status = uefi_call_wrapper(kernel_img_file->Read, 3, kernel_img_file, &buffer_read_size, *kernel_program_headers_buffer);
    if (EFI_ERROR(status))
    {
        Log("Error: Error reading kernel program headers: %r", status);
        return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS read_elf_identity(IN EFI_FILE *const kernel_img_file, OUT UINT8 **elf_identity_buffer)
{
    /** The amount of bytes to read into the buffer. */
    UINTN buffer_read_size = EI_NIDENT;

#if MOS_LOADER_DEBUG
    Log("Debug: Setting file pointer position to read ELF identity");
#endif

    // Reset to the start of the file.
    EFI_STATUS status = uefi_call_wrapper(kernel_img_file->SetPosition, 2, kernel_img_file, 0);
    if (EFI_ERROR(status))
    {
        Log("Error: Error resetting file pointer position: %r", status);
        return status;
    }

#if MOS_LOADER_DEBUG
    Log("Debug: Allocating buffer for ELF identity");
#endif

    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, EI_NIDENT, (VOID **) elf_identity_buffer);
    if (EFI_ERROR(status))
    {
        Log("Error: Error allocating kernel identity buffer: %r", status);
        return status;
    }

#if MOS_LOADER_DEBUG
    Log("Debug: Reading ELF identity");
#endif

    status = uefi_call_wrapper(kernel_img_file->Read, 3, kernel_img_file, &buffer_read_size, (VOID *) *elf_identity_buffer);
    if (EFI_ERROR(status))
    {
        Log("Error: Error reading kernel identity: %r", status);
        return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS validate_elf_identity(IN UINT8 *const elf_identity_buffer)
{
    if ((elf_identity_buffer[EI_MAG0] != 0x7F) || (elf_identity_buffer[EI_MAG1] != 0x45) || (elf_identity_buffer[EI_MAG2] != 0x4C) ||
        (elf_identity_buffer[EI_MAG3] != 0x46))
    {
        Log("Fatal Error: Invalid ELF header");
        return EFI_INVALID_PARAMETER;
    }

    if (elf_identity_buffer[EI_CLASS] == ELF_FILE_CLASS_32)
    {
#if MOS_LOADER_DEBUG
        Log("Debug: Found 32bit executable");
#endif
    }
    else if (elf_identity_buffer[EI_CLASS] == ELF_FILE_CLASS_64)
    {
#if MOS_LOADER_DEBUG
        Log("Debug: Found 64bit executable");
#endif
    }
    else
    {
        Log("Fatal Error: Invalid executable");
        return EFI_UNSUPPORTED;
    }

    if (elf_identity_buffer[EI_DATA] != 1)
    {
        Log("Fatal Error: Only LSB ELF executables current supported");
        return EFI_INCOMPATIBLE_VERSION;
    }

    return EFI_SUCCESS;
}
