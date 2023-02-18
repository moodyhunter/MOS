// SPDX-License-Identifier: GPL-3.0-or-later

#include "common.h"

const CHAR16 *const memtypes[] = {
    L"Reserved",           //
    L"Loader-C",           //
    L"Loader-D",           //
    L"Boot Services-C",    //
    L"Boot Services-D",    //
    L"Runtime Services-C", //
    L"Runtime Services-D", //
    L"Conventional",       //
    L"Unusable",           //
    L"ACPI Reclaim",       //
    L"ACPI MemoryNVS",     //
};

EFI_STATUS bl_dump_memmap(void)
{
    // dump the memory map
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_size = 0, map_key = 0, desc_size = 0;
    UINT32 desc_version = 0;
    EFI_STATUS status = uefi_call_wrapper(BS->GetMemoryMap, 5, &map_size, map, &map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL)
    {
        Log("Failed to get memory map size: %r", status);
        return status;
    }

    Log("Memory map size: %ld, map key: %ld, desc size: %ld, desc version: %d", map_size, map_key, desc_size, desc_version);

    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, map_size, &map);
    if (EFI_ERROR(status))
    {
        Log("Failed to allocate memory for memory map: %r", status);
        return status;
    }

    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &map_size, map, &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status))
    {
        if (status == EFI_BUFFER_TOO_SMALL)
        {
            // reallocate the memory map
            status = uefi_call_wrapper(BS->FreePool, 1, map);
            if (EFI_ERROR(status))
                return status;

            status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, map_size, &map);
            if (EFI_ERROR(status))
            {
                Log("Failed to allocate memory for memory map: %r", status);
                return status;
            }

            status = uefi_call_wrapper(BS->GetMemoryMap, 5, &map_size, map, &map_key, &desc_size, &desc_version);
            if (EFI_ERROR(status))
            {
                Log("Failed to get memory map: %r, map_size=%ld", status, map_size);
                return status;
            }

            goto success;
        }

        Log("Failed to get memory map: %r, map_size=%ld", status, map_size);
        return status;
    }

success:;

    Log("Memory map:");

    CHAR16 *cache_type = NULL, *memprotection = NULL;
    // 64 bytes should be enough
    uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, 64, &cache_type);
    uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, 64, &memprotection);

    for (UINTN i = 0; i < map_size / desc_size; i++)
    {
        cache_type[0] = L'\0';
        memprotection[0] = L'\0';

        const EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR *) ((UINT8 *) map + i * desc_size);
        const UINT64 pbegin = d->PhysicalStart;
        const UINT64 pend = d->PhysicalStart + d->NumberOfPages * EFI_PAGE_SIZE;
        const UINT64 vbegin = d->VirtualStart;
        const UINT64 vend = d->VirtualStart + d->NumberOfPages * EFI_PAGE_SIZE;

        if (d->Attribute & EFI_MEMORY_UC)
            StrCat(cache_type, L"UC ");
        if (d->Attribute & EFI_MEMORY_WC)
            StrCat(cache_type, L"WC ");
        if (d->Attribute & EFI_MEMORY_WT)
            StrCat(cache_type, L"WT ");
        if (d->Attribute & EFI_MEMORY_WB)
            StrCat(cache_type, L"WB ");
        if (d->Attribute & EFI_MEMORY_UCE)
            StrCat(cache_type, L"UCE ");

        if (d->Attribute & EFI_MEMORY_WP)
            StrCat(memprotection, L"WP ");
        if (d->Attribute & EFI_MEMORY_RP)
            StrCat(memprotection, L"RP ");
        if (d->Attribute & EFI_MEMORY_XP)
            StrCat(memprotection, L"XP ");

        const CHAR16 *const runtime = (d->Attribute & EFI_MEMORY_RUNTIME) ? L"runtime" : L"";

        Log("  0x%016lx - 0x%016lx --> v 0x%016lx - 0x%016lx, %10ld pages, %20s, %s%s%s", //
            pbegin,                                                                       //
            pend,                                                                         //
            vbegin,                                                                       //
            vend,                                                                         //
            d->NumberOfPages,                                                             //
            memtypes[d->Type],                                                            //
            cache_type,                                                                   //
            memprotection,                                                                //
            runtime                                                                       //
        );
    }

    uefi_call_wrapper(BS->FreePool, 1, cache_type);
    uefi_call_wrapper(BS->FreePool, 1, memprotection);

    // deallocate the memory map
    status = uefi_call_wrapper(BS->FreePool, 1, map);
    if (EFI_ERROR(status))
        return status;

    return EFI_SUCCESS;
}
