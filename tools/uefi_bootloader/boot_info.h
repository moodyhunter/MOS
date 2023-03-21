// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifndef MOS_EFI_LOADER
#include <mos/types.h>
#define CHAR16 u16
#define UINT32 u32
#define UINTN  nuint
typedef struct
{
    u32 Type;
    u32 Pad;
    uintptr_t PhysicalStart;
    uintptr_t VirtualStart;
    u64 NumberOfPages;
    u64 Attribute;
} EFI_MEMORY_DESCRIPTOR;
#else
#include <efi/efi.h>
#endif

typedef struct
{
    EFI_MEMORY_DESCRIPTOR *mapptr;
    UINTN key;
    UINTN size;
    UINTN descriptor_size;
    UINT32 version;
} efi_memory_map_info_t;

typedef struct
{
    efi_memory_map_info_t memory_map;
    CHAR16 *kernel;
    CHAR16 *cmdline;
} boot_info_t;

#ifndef MOS_EFI_LOADER
#undef CHAR16
#undef UINT32
#undef UINTN
#endif
