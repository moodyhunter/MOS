// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "efi.h"
#include "efilib.h"

#include <stdbool.h>
#include <stdint.h>

#define Log(fmt, ...) Print(L"" fmt "\r\n", ##__VA_ARGS__)

#define MOS_LOADER_CMDLINE L"mos_cmdline.txt"
#define MOS_LOADER_KERNEL  L"mos_kernel.bin"

extern const CHAR16 *const memtypes[];

extern EFI_STATUS bl_load_cmdline(EFI_HANDLE image, CHAR16 **cmdline, const CHAR16 *const file_name);
extern EFI_STATUS bl_dump_memmap(void);
