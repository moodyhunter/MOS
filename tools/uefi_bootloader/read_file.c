// SPDX-License-Identifier: GPL-3.0-or-later

#include "common.h"

EFI_STATUS bl_load_cmdline_from_file(EFI_HANDLE image, CHAR16 **cmdline, const CHAR16 *const file_name)
{
    EFI_LOADED_IMAGE *loaded_image = NULL;
    EFI_STATUS status = uefi_call_wrapper(BS->OpenProtocol, 6, image, &LoadedImageProtocol, &loaded_image, image, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(status))
    {
        Log("Failed to open the loaded image protocol: %r", status);
        return status; // nothing to clean up
    }

    EFI_DEVICE_PATH *device_path = loaded_image->FilePath;
    if (!device_path)
    {
        Log("Failed to get the device path");
        status = EFI_INVALID_PARAMETER;
        goto cleanup_close_loaded_image;
    }

    EFI_FILE_IO_INTERFACE *file_io = NULL;
    status = uefi_call_wrapper(BS->OpenProtocol, 6, loaded_image->DeviceHandle, &FileSystemProtocol, &file_io, image, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(status))
    {
        Log("Failed to open the file system protocol: %r", status);
        goto cleanup_close_loaded_image;
    }

    EFI_FILE *root_dir = NULL;
    status = uefi_call_wrapper(file_io->OpenVolume, 2, file_io, &root_dir);
    if (EFI_ERROR(status))
    {
        Log("Failed to open the root directory: %r", status);
        goto cleanup_close_file_io;
    }

    EFI_FILE *file = NULL;
    status = uefi_call_wrapper(root_dir->Open, 5, root_dir, &file, file_name, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status))
    {
        Log("Failed to open the file: %r", status);
        goto cleanup_close_root_dir;
    }

    UINTN file_size = 0;
    status = uefi_call_wrapper(file->SetPosition, 2, file, 0);
    if (EFI_ERROR(status))
    {
        Log("Failed to set the file position: %r", status);
        goto cleanup_close_file;
    }

    status = uefi_call_wrapper(file->GetInfo, 4, file, &GenericFileInfo, &file_size, NULL);
    if (EFI_ERROR(status))
    {
        if (status != EFI_BUFFER_TOO_SMALL)
        {
            Log("Failed to get the file info: %r", status);
            goto cleanup_close_file;
        }
    }

    EFI_FILE_INFO *file_info;
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, file_size, (void **) &file_info);
    if (!file_info)
    {
        Log("Failed to allocate memory for the file info");
        status = EFI_OUT_OF_RESOURCES;
        goto cleanup_close_file;
    }

    status = uefi_call_wrapper(file->GetInfo, 4, file, &GenericFileInfo, &file_size, file_info);
    if (EFI_ERROR(status))
    {
        Log("Failed to get the file info: %r", status);
        goto cleanup_dealloc_file_info;
    }

    UINTN cmdline_size = file_info->FileSize + sizeof(CHAR16);
    uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, cmdline_size, (void **) cmdline);
    if (!*cmdline)
    {
        Log("Failed to allocate memory for the command line");
        status = EFI_OUT_OF_RESOURCES;
        goto cleanup_dealloc_file_info;
    }

    status = uefi_call_wrapper(file->Read, 3, file, &cmdline_size, *cmdline);
    if (EFI_ERROR(status))
    {
        Log("Failed to read the file: %r", status);
        goto cleanup_dealloc_cmdline;
    }

    // remove the leading BOM if present
    if (cmdline_size >= 3 && (*cmdline)[0] == 0xFEFF)
    {
        for (UINTN i = 0; i < cmdline_size - 1; i++)
            (*cmdline)[i] = (*cmdline)[i + 1];

        cmdline_size -= sizeof(CHAR16);
    }

    // ensure the command line is null terminated
    (*cmdline)[cmdline_size / sizeof(CHAR16)] = 0L;

    // remove the trailing newline if present
    while (true)
    {
        if (cmdline_size == 0)
            break;

        if ((*cmdline)[cmdline_size / sizeof(CHAR16) - 1] != L'\r' && (*cmdline)[cmdline_size / sizeof(CHAR16) - 1] != L'\n')
            break;

        (*cmdline)[cmdline_size / sizeof(CHAR16) - 1] = 0L;
        cmdline_size -= sizeof(CHAR16);

        if (cmdline_size == 0)
            break;
    }

    status = EFI_SUCCESS;
    goto cleanup_dealloc_file_info; // don't free the cmdline buffer

cleanup_dealloc_cmdline:
    uefi_call_wrapper(BS->FreePages, 2, (EFI_PHYSICAL_ADDRESS) *cmdline);

cleanup_dealloc_file_info:
    uefi_call_wrapper(BS->FreePages, 2, (EFI_PHYSICAL_ADDRESS) file_info);

cleanup_close_file:
    uefi_call_wrapper(file->Close, 1, file);

cleanup_close_root_dir:
    uefi_call_wrapper(root_dir->Close, 1, root_dir);

cleanup_close_file_io:
    uefi_call_wrapper(BS->CloseProtocol, 4, loaded_image->DeviceHandle, &FileSystemProtocol, image, NULL);

cleanup_close_loaded_image:
    uefi_call_wrapper(BS->CloseProtocol, 4, image, &LoadedImageProtocol, image, NULL);

    return status;
}
