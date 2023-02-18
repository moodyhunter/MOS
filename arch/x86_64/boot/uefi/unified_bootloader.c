// SPDX-License-Identifier: GPL-3.0-or-later
#include "efi.h"
#include "efilib.h"

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *table)
{
    InitializeLib(image, table);
    ST = table;

    extern char _data, _text;

    Print(L"MOS UEFI Bootloader '" __DATE__ "-" __TIME__ "', built with GCC '" __VERSION__ "'\n");
    Print(L"====================\n");
    Print(L"Use the following command to attach a debugger:\n");
    Print(L"add-symbol-file build/arch/x86_64/boot/uefi/mos_uefi_loader.debug 0x%lx -s .data 0x%lx\n", &_text, &_data);
    Print(L"====================\n");

    return EFI_SUCCESS;
}
