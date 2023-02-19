// SPDX-License-Identifier: GPL-3.0-or-later

#include "common.h"

#define MOS_LOADER_CMDLINE_FILE L"\\mos_cmdline.txt"
#define MOS_LOADER_KERNEL_FILE  L"\\mos_kernel.elf"

static struct
{
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *protocol;
} file_system_service = { 0 };

/**
 * @brief Allocates the memory map.
 * Allocates the memory map. This function needs to be run prior to exiting
 * UEFI boot services.
 * @param[out] memory_map            A pointer to pointer to the memory map buffer to be allocated in this function.
 * @param[out] memory_map_size       The size of the allocated buffer.
 * @param[out] memory_map_key        They key of the allocated memory map.
 * @param[out] descriptor_size       A pointer to the size of an individual EFI_MEMORY_DESCRIPTOR.
 * @param[out] descriptor_version    A pointer to the version number associated with the EFI_MEMORY_DESCRIPTOR.
 * @return                   The program status.
 * @retval EFI_SUCCESS       The function executed successfully.
 * @retval other             A fatal error occurred getting the memory map.
 * @warning After this function has been run, no other boot services may be used otherwise the memory map will be considered invalid.
 */
EFI_STATUS get_memory_map(IN OUT efi_memory_map_info_t *mmapinfo)
{
    EFI_STATUS status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mmapinfo->size, mmapinfo->mapptr, &mmapinfo->key, &mmapinfo->descriptor_size, &mmapinfo->version);
    if (EFI_ERROR(status))
    {
        // This will always fail on the first attempt, this call will return the required buffer size.
        if (status != EFI_BUFFER_TOO_SMALL)
        {
            Log("Fatal Error: Error getting memory map size: %r", status);
            return status;
        }
    }

    // According to: https://stackoverflow.com/a/39674958/5931673
    // Up to two new descriptors may be created in the process of allocating the new pool memory.
    mmapinfo->size += 2 * mmapinfo->descriptor_size;

    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, mmapinfo->size, &mmapinfo->mapptr);
    if (is_fatal_error(status, L"allocating memory map buffer"))
        return status;

    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mmapinfo->size, mmapinfo->mapptr, &mmapinfo->key, &mmapinfo->descriptor_size, &mmapinfo->version);
    if (is_fatal_error(status, L"getting memory map"))
        return status;

    return EFI_SUCCESS;
}

EFI_STATUS bl_get_params(IN EFI_HANDLE image, OUT CHAR16 **kernel_path, OUT CHAR16 **cmdline)
{
    CHAR16 **argv = NULL;
    INTN argc = GetShellArgcArgv(image, &argv);

    // ./loader.efi
    // ./loader.efi kernel cmdline_file.txt
    // ./loader.efi kernel -- [cmdline ...]

    switch (argc)
    {
        case 1:
        {
            Log("Loading MOS kernel and initrd from the default locations...");
            Log("Kernel: '%s'", MOS_LOADER_KERNEL_FILE);
            Log("Command Line File: '%s'", MOS_LOADER_CMDLINE_FILE);
            *kernel_path = StrDuplicate(MOS_LOADER_KERNEL_FILE);
            EFI_STATUS status = bl_load_cmdline_from_file(image, cmdline, MOS_LOADER_CMDLINE_FILE);
            if (EFI_ERROR(status))
            {
                Log("Failed to load command line: %r", status);
                return status;
            }

            break;
        }
        case 2:
        {
            Log("Invalid number of arguments: %d", argc);
            return EFI_INVALID_PARAMETER;
        }
        default:
        {
            *kernel_path = StrDuplicate(argv[1]);
            Log("Kernel: '%s'", *kernel_path);

            if (StrCmp(argv[2], L"--") == 0)
            {
                Log("Command Line: '%s'", "concatenated arguments");
                // ./loader.efi kernel -- [cmdline ...]
                // concatenate the rest of the arguments

                // count the number of arguments
                const INTN cmdline_argc = argc - 3;

                // allocate memory for the command line
                CHAR16 *cmdline_buf = NULL;
                uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, cmdline_argc * 256, &cmdline_buf);
                cmdline_buf[0] = L'\0';

                // concatenate the arguments
                for (INTN i = 3; i < argc; i++)
                {
                    StrCat(cmdline_buf, argv[i]);
                    StrCat(cmdline_buf, L" ");
                }
                // remove the trailing space
                cmdline_buf[StrLen(cmdline_buf) - 1] = L'\0';

                *cmdline = cmdline_buf;
            }
            else
            {
                if (argc > 3)
                {
                    Log("Invalid number of arguments: %d", argc);
                    return EFI_INVALID_PARAMETER;
                }

                // ./loader.efi kernel cmdline_file.txt
                Log("Command Line: '%s'", argv[2]);
                CHAR16 *cmdline_buf = NULL;
                EFI_STATUS status = bl_load_cmdline_from_file(image, &cmdline_buf, argv[2]);
                if (is_fatal_error(status, L"loading command line"))
                    return status;
                *cmdline = cmdline_buf;
            }
            break;
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);
    ST = SystemTable;
    BS = SystemTable->BootServices;
    RT = SystemTable->RuntimeServices;

    Log("MOS Loader '" __DATE__ " " __TIME__ "'");

    extern char _data, _text;

    Log("MOS UEFI Bootloader '" __DATE__ "-" __TIME__ "', built with GCC '" __VERSION__ "'");
    Log("====================");
    Log("Use the following command to attach a debugger:");
    Log("add-symbol-file build/arch/x86_64/boot/uefi/mos_uefi_loader.debug 0x%lx -s .data 0x%lx", &_text, &_data);
    Log("====================");

    EFI_STATUS status;
    status = uefi_call_wrapper(BS->SetWatchdogTimer, 4, 0, 0, 0, NULL);
    if (is_fatal_error(status, L"Error setting watchdog timer"))
        return status;

    Log("Watchdog timer disabled.");

    // Reset console input.
    status = uefi_call_wrapper(ST->ConIn->Reset, 2, SystemTable->ConIn, FALSE);
    if (is_fatal_error(status, L"Error resetting console input"))
        return status;

    Log("Console input reset.");

    status = uefi_call_wrapper(BS->LocateProtocol, 3, &gEfiSimpleFileSystemProtocolGuid, NULL, &file_system_service.protocol);
    if (is_fatal_error(status, L"Error locating Simple File System Protocol"))
        return status;

    Log("Simple File System Protocol located.");

    EFI_FILE *root_file_system = NULL;
    status = uefi_call_wrapper(file_system_service.protocol->OpenVolume, 2, file_system_service.protocol, &root_file_system);
    if (is_fatal_error(status, L"Error opening root volume"))
        return status;

    Log("Root volume opened.");

    boot_info_t bootinfo = { 0 };

    status = bl_get_params(ImageHandle, &bootinfo.kernel, &bootinfo.cmdline);
    if (is_fatal_error(status, L"Error getting parameters"))
        return status;

    EFI_PHYSICAL_ADDRESS *kernel_entry_point = 0;
    status = load_kernel_image(root_file_system, bootinfo.kernel, kernel_entry_point);
    if (EFI_ERROR(status))
        return status;

    Log("Kernel Entry: 0x%llx", *kernel_entry_point);

    // !
    // ! Do not print anything after this point, as the memory map will be invalid.
    // !
    Log("Leaving boot services...");
    status = get_memory_map(&bootinfo.memory_map);
    if (EFI_ERROR(status))
        return status;
    status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, bootinfo.memory_map.key);
    if (is_fatal_error(status, L"Error exiting boot services"))
        return status;

    const kernel_entry_t kernel_entry = (const kernel_entry_t)(*kernel_entry_point);
    kernel_entry(&bootinfo);

    while (true)
        __asm__ __volatile__("hlt");

    // unreachable
    return EFI_LOAD_ERROR;
}
