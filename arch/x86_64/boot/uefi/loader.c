/**
 * @file loader.c
 * @author ajxs
 * @date Aug 2019
 * @brief Functionality for loading the Kernel executable.
 * Contains functionality for loading the Kernel ELF executable.
 */

#include "common.h"
#include "elf.h"

EFI_STATUS load_segment(IN EFI_FILE *const kernel_img_file, IN EFI_PHYSICAL_ADDRESS const segment_file_offset, IN UINTN const segment_file_size,
                        IN UINTN const segment_memory_size, IN EFI_PHYSICAL_ADDRESS const segment_physical_address)
{
    EFI_STATUS status;                                                 // Program status
    VOID *program_data = NULL;                                         // Buffer to hold the segment data.
    UINTN buffer_read_size = 0;                                        // The amount of data to read into the buffer.
    UINTN segment_page_count = EFI_SIZE_TO_PAGES(segment_memory_size); // The number of pages to allocate.
    EFI_PHYSICAL_ADDRESS zero_fill_start = 0;                          // The memory location to begin zero filling empty segment space.
    UINTN zero_fill_count = 0;                                         // The number of bytes to zero fill.

#if MOS_LOADER_DEBUG
    Log("Debug: Setting file pointer to segment "
        "offset '0x%llx'",
        segment_file_offset);
#endif

    status = uefi_call_wrapper(kernel_img_file->SetPosition, 2, kernel_img_file, segment_file_offset);
    if (is_fatal_error(status, L"Error setting file pointer to segment offset"))
        return status;

#if MOS_LOADER_DEBUG
    Log("Debug: Allocating %lu pages at address '0x%llx'", segment_page_count, segment_physical_address);
#endif

    status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, segment_page_count, (EFI_PHYSICAL_ADDRESS *) &segment_physical_address);
    if (is_fatal_error(status, L"Error allocating pages for ELF segment"))
        return status;

    if (segment_file_size > 0)
    {
        buffer_read_size = segment_file_size;

#if MOS_LOADER_DEBUG
        Log("Debug: Allocating segment buffer with size '0x%llx'", buffer_read_size);
#endif

        status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderCode, buffer_read_size, (VOID **) &program_data);
        if (is_fatal_error(status, L"Error allocating kernel segment buffer"))
            return status;

#if MOS_LOADER_DEBUG
        Log("Debug: Reading segment data with file size '0x%llx'", buffer_read_size);
#endif

        status = uefi_call_wrapper(kernel_img_file->Read, 3, kernel_img_file, &buffer_read_size, (VOID *) program_data);
        if (is_fatal_error(status, L"Error reading segment data"))
            return status;

#if MOS_LOADER_DEBUG
        Log("Debug: Copying segment to memory address '0x%llx'", segment_physical_address);
#endif

        status = uefi_call_wrapper(BS->CopyMem, 3, segment_physical_address, program_data, segment_file_size);
        if (is_fatal_error(status, L"Error copying program section into memory"))
            return status;

#if MOS_LOADER_DEBUG
        Log("Debug: Freeing program section data buffer");
#endif

        status = uefi_call_wrapper(BS->FreePool, 1, program_data);
        if (is_fatal_error(status, L"Error freeing program section"))
            return status;
    }

    // As per ELF Standard, if the size in memory is larger than the file size
    // the segment is mandated to be zero filled.
    // For more information on Refer to ELF standard page 34.
    zero_fill_start = segment_physical_address + segment_file_size;
    zero_fill_count = segment_memory_size - segment_file_size;

    if (zero_fill_count > 0)
    {
#if MOS_LOADER_DEBUG
        Log("Debug: Zero-filling %llu bytes at address '0x%llx'", zero_fill_count, zero_fill_start);
#endif
        status = uefi_call_wrapper(BS->SetMem, 3, zero_fill_start, zero_fill_count, 0);
        if (is_fatal_error(status, L"Error zero filling segment"))
            return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS load_program_segments(IN EFI_FILE *const kernel_img_file, IN Elf_File_Class const file_class, IN VOID *const kernel_header_buffer,
                                 IN VOID *const kernel_program_headers_buffer)
{
    EFI_STATUS status;
    UINT16 n_program_headers = 0;
    UINT16 n_segments_loaded = 0;

    if (file_class == ELF_FILE_CLASS_32)
        n_program_headers = ((Elf32_Ehdr *) kernel_header_buffer)->e_phnum;
    else if (file_class == ELF_FILE_CLASS_64)
        n_program_headers = ((Elf64_Ehdr *) kernel_header_buffer)->e_phnum;

    // Exit if there are no executable sections in the kernel image.
    if (n_program_headers == 0)
    {
        Log("Fatal Error: No program segments to load in Kernel image");

        return EFI_INVALID_PARAMETER;
    }

#if MOS_LOADER_DEBUG
    Log("Debug: Loading %u segments", n_program_headers);
#endif

    if (file_class == ELF_FILE_CLASS_32)
    {
        /** Program headers pointer. */
        Elf32_Phdr *program_headers = (Elf32_Phdr *) kernel_program_headers_buffer;

        for (UINTN p = 0; p < n_program_headers; p++)
        {
            if (program_headers[p].p_type == PT_LOAD)
            {
                status = load_segment(kernel_img_file, program_headers[p].p_offset, program_headers[p].p_filesz, program_headers[p].p_memsz, program_headers[p].p_paddr);
                if (EFI_ERROR(status))
                    return status;
                n_segments_loaded++;
            }
        }
    }
    else if (file_class == ELF_FILE_CLASS_64)
    {
        /** Program headers pointer. */
        Elf64_Phdr *program_headers = (Elf64_Phdr *) kernel_program_headers_buffer;

        for (UINTN p = 0; p < n_program_headers; p++)
        {
            if (program_headers[p].p_type == PT_LOAD)
            {
                status = load_segment(kernel_img_file, program_headers[p].p_offset, program_headers[p].p_filesz, program_headers[p].p_memsz, program_headers[p].p_paddr);
                if (EFI_ERROR(status))
                    return status;
                n_segments_loaded++;
            }
        }
    }

    // If we have found no loadable segments, raise an exception.
    if (n_segments_loaded == 0)
    {
        Log("Fatal Error: No loadable program segments found in Kernel image");
        return EFI_NOT_FOUND;
    }

    return EFI_SUCCESS;
}

EFI_STATUS load_kernel_image(IN EFI_FILE *const root_file_system, IN CHAR16 *const kernel_image_filename, OUT EFI_PHYSICAL_ADDRESS *kernel_entry_point)
{
    EFI_STATUS status;
    EFI_FILE *kernel_img_file;
    VOID *kernel_header = NULL;
    VOID *kernel_program_headers = NULL;
    UINT8 *elf_identity_buffer = NULL;
    Elf_File_Class file_class = ELF_FILE_CLASS_NONE;

#if MOS_LOADER_DEBUG
    Log("Debug: Reading kernel image file");
#endif

    status = uefi_call_wrapper(root_file_system->Open, 5, root_file_system, &kernel_img_file, kernel_image_filename, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
    if (is_fatal_error(status, L"Error opening kernel file"))
        return status;

    // Read ELF Identity.
    // From here we can validate the ELF executable, as well as determine the
    // file class.
    status = read_elf_identity(kernel_img_file, &elf_identity_buffer);
    if (is_fatal_error(status, L"Error reading executable identity"))
        return status;

    file_class = elf_identity_buffer[EI_CLASS];

    // Validate the ELF file.
    status = validate_elf_identity(elf_identity_buffer);
    if (is_fatal_error(status, L"Error validating ELF file"))
        return status;

#if MOS_LOADER_DEBUG
    Log("Debug: ELF header is valid");
#endif

    // Free identity buffer.
    status = uefi_call_wrapper(BS->FreePool, 1, elf_identity_buffer);
    if (is_fatal_error(status, L"Error freeing kernel identity buffer"))
        return status;

    // Read the ELF file and program headers.
    status = read_elf_file(kernel_img_file, file_class, &kernel_header, &kernel_program_headers);
    if (is_fatal_error(status, L"Error reading ELF file"))
        return status;

#if MOS_LOADER_DEBUG
    print_elf_file_info(kernel_header, kernel_program_headers);
#endif

    // Set the kernel entry point to the address specified in the ELF header.
    if (file_class == ELF_FILE_CLASS_32)
        *kernel_entry_point = ((Elf32_Ehdr *) kernel_header)->e_entry;
    else if (file_class == ELF_FILE_CLASS_64)
        *kernel_entry_point = ((Elf64_Ehdr *) kernel_header)->e_entry;

    status = load_program_segments(kernel_img_file, file_class, kernel_header, kernel_program_headers);
    if (EFI_ERROR(status))
        return status;

    // Cleanup.
    status = uefi_call_wrapper(kernel_img_file->Close, 1, kernel_img_file);
    if (is_fatal_error(status, L"Error closing kernel image"))
        return status;

    status = uefi_call_wrapper(BS->FreePool, 1, (VOID *) kernel_header);
    if (is_fatal_error(status, L"Error freeing kernel header buffer"))
        return status;

    status = uefi_call_wrapper(BS->FreePool, 1, (VOID *) kernel_program_headers);
    if (is_fatal_error(status, L"Error freeing kernel program headers buffer"))
        return status;
    return status;
}
