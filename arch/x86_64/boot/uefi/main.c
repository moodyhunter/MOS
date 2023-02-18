// SPDX-License-Identifier: GPL-3.0-or-later
#include "common.h"

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *table)
{
    ST = table;
    BS = table->BootServices;
    RT = table->RuntimeServices;
    InitializeLib(image, table);

#ifdef MOS_LOADER_DEBUG
    extern char _data, _text;

    Log("MOS UEFI Bootloader '" __DATE__ "-" __TIME__ "', built with GCC '" __VERSION__ "'");
    Log("====================");
    Log("Use the following command to attach a debugger:");
    Log("add-symbol-file build/arch/x86_64/boot/uefi/mos_uefi_loader.debug 0x%lx -s .data 0x%lx", &_text, &_data);
    Log("====================");
#endif

    // get argc and argv
    CHAR16 **argv = NULL;
    INTN argc = GetShellArgcArgv(image, &argv);

    // ./loader.efi
    // ./loader.efi kernel cmdline_file.txt
    // ./loader.efi kernel -- [cmdline ...]

    CHAR16 *cmdline = NULL;
    CHAR16 *kernel_path = NULL;

    switch (argc)
    {
        case 1:
        {
            Log("Loading MOS kernel and initrd from the default locations...");
            kernel_path = MOS_LOADER_KERNEL;

            EFI_STATUS status = bl_load_cmdline(image, &cmdline, MOS_LOADER_CMDLINE);
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
            kernel_path = argv[1];

            if (StrCmp(argv[2], L"--") == 0)
            {
                // ./loader.efi kernel -- [cmdline ...]
                // concatenate the rest of the arguments

                // count the number of arguments
                const INTN cmdline_argc = argc - 3;

                // allocate memory for the command line
                uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, cmdline_argc * 256, &cmdline);
                cmdline[0] = L'\0';

                // concatenate the arguments
                for (INTN i = 3; i < argc; i++)
                {
                    StrCat(cmdline, argv[i]);
                    StrCat(cmdline, L" ");
                }
                // remove the trailing space
                cmdline[StrLen(cmdline) - 1] = L'\0';
            }
            else
            {
                EFI_STATUS status = bl_load_cmdline(image, &cmdline, argv[2]);
                if (EFI_ERROR(status))
                {
                    Log("Failed to load the command line: %r", status);
                    return status;
                }
            }
            break;
        }
    }

    Log("Kernel: '%s'", kernel_path);
    Log("Command line: '%s'", cmdline);
    return EFI_SUCCESS;

    EFI_STATUS status = bl_dump_memmap();
    if (EFI_ERROR(status))
    {
        Log("Failed to dump the memory map: %r", status);
        return status;
    }
    return EFI_SUCCESS;
}
