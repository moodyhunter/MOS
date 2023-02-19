// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "boot_info.h"
#include "elf.h"

#include <efi/efi.h>
#include <efi/efilib.h>
#include <stdbool.h>

#define MOS_LOADER_DEBUG 0

#define Log(fmt, ...) Print(L"" fmt "\n", ##__VA_ARGS__)

typedef void (*kernel_entry_t)(boot_info_t *boot_info);

/**
 * @file common.h
 * @author ajxs
 * @date Aug 2019
 * @brief Functionality for loading the Kernel executable.
 * Contains functionality for loading the Kernel ELF executable.
 */

/**
 * @brief Tests a status variable to determine whether an EFI error has occurred, and
 * prints the specified error message if so.
 * @param[in] status The status code to test.
 * @param[in] error_message The error message to print in the case that the status
 * code represents an error.
 * @return A boolean indicating whether an error has occurred.
 */
__attribute__((unused)) static inline bool is_fatal_error(IN EFI_STATUS const status, IN CONST CHAR16 *error_message)
{
    const bool is_error = EFI_ERROR(status);
    if (is_error)
        Log("Fatal Error: When %s: %r", error_message, status);
    return is_error;
}

EFI_STATUS bl_load_cmdline_from_file(EFI_HANDLE image, CHAR16 **cmdline, const CHAR16 *const file_name);

/**
 * @brief Loads an ELF segment.
 * Loads an ELF program segment into memory.
 * This will read the ELF segment from the kernel binary, allocate the pages
 * necessary to load the segment into memory and then copy the segment to its
 * required physical memory address.
 * @param[in] kernel_img_file The Kernel EFI file entity to read from.
 * @param[in] segment_file_offset The segment's offset into the ELF binary.
 * @param[in] segment_file_size The segment's size in the ELF binary.
 * @param[in] segment_memory_size The size of the segment loaded into memory.
 * @param[in] segment_physical_address The physical memory address to load the segment to.
 * @return The program status.
 * @retval EFI_SUCCESS    If the function executed successfully.
 * @retval other          Any other value is an EFI error code.
 */
EFI_STATUS load_segment(IN EFI_FILE *const file, IN EFI_PHYSICAL_ADDRESS const segment_file_offset, IN UINTN const segment_file_size, IN UINTN const segment_memory_size,
                        IN EFI_PHYSICAL_ADDRESS const segment_physical_address);

/**
 * @brief Loads the ELF program segments.
 * Loads the Kernel ELF binary's program segments into memory.
 * @param[in] kernel_img_file The Kernel EFI file entity to read from.
 * @param[in] file_class The ELF file class, whether the program is 32 or 64bit.
 * @param[in] kernel_header_buffer The Kernel header buffer.
 * @param[in] kernel_program_headers_buffer The kernel program headers buffer.
 * @return The program status.
 * @retval EFI_SUCCESS    If the function executed successfully.
 * @retval other          Any other value is an EFI error code.
 */
EFI_STATUS load_program_segments(IN EFI_FILE *const file, IN Elf_File_Class const file_class, IN VOID *const kernel_header_buffer,
                                 IN VOID *const kernel_program_headers_buffer);

/**
 * @brief Loads the Kernel binary image into memory.
 * This will load the Kernel binary image and validates it. If the kernel binary
 * is valid its executable program segments are loaded into memory.
 * @param[in]   root_file_system The root file system FILE entity to load the
 *              kernel binary from.
 * @param[in]   kernel_image_filename The kernel filename on the boot partition.
 * @param[out]  kernel_entry_point The entry point memory address for
 *              the kernel.
 * @return The program status.
 * @retval EFI_SUCCESS    If the function executed successfully.
 * @retval other          Any other value is an EFI error code.
 */
EFI_STATUS load_kernel_image(IN EFI_FILE *const root_file_system, IN CHAR16 *const kernel_image_filename, OUT EFI_PHYSICAL_ADDRESS *kernel_entry_point);
