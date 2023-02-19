# SPDX-License-Identifier: GPL-3.0-or-later

project(mos_uefi_bootloader C ASM)

find_program(SYSTEM_GCC gcc REQUIRED)
set(CMAKE_C_COMPILER ${SYSTEM_GCC})
set(CMAKE_C_FLAGS "-Wall -Wextra")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_library(gnuefi_libgnuefi NAMES "libgnuefi.a" REQUIRED)
find_library(gnuefu_libefi NAMES "libefi.a" REQUIRED)
find_path(gnuefi_include_dir "efi.h" REQUIRED PATH_SUFFIXES "include" "include/gnuefi" "include/efi")
find_file(gnuefi_crt0 "crt0-efi-${MOS_ARCH}.o" REQUIRED PATH_SUFFIXES "lib")
find_file(gnuefi_lds "elf_${MOS_ARCH}_efi.lds" REQUIRED PATH_SUFFIXES "lib")

add_library(gnuefi INTERFACE IMPORTED)
set_target_properties(gnuefi PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${gnuefi_include_dir}"
    INTERFACE_COMPILE_OPTIONS "-ffreestanding;-fshort-wchar;-mno-red-zone;-ggdb"
    INTERFACE_LINK_OPTIONS "-nostdlib;-Wl,-Bsymbolic;-znocombreloc;-T;${gnuefi_lds}"
    INTERFACE_LINK_LIBRARIES "${gnuefi_crt0};${gnuefi_libgnuefi};${gnuefu_libefi}"
)

add_library(mos_uefi_loader SHARED
    elf.c
    loader.c
    main.c
    dump_mem.c
    read_file.c
)

target_compile_options(mos_uefi_loader PRIVATE -Wno-strict-prototypes -Wno-pedantic) # GNU-EFI produces warnings
target_compile_definitions(mos_uefi_loader PRIVATE -DMOS_EFI_LOADER)
target_link_libraries(mos_uefi_loader gnuefi)
target_include_directories(mos_uefi_loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(UEFI_LOADER_BINARY ${CMAKE_CURRENT_BINARY_DIR}/mos_uefi_loader.efi)

add_custom_command(TARGET mos_uefi_loader
    POST_BUILD
    BYPRODUCTS ${UEFI_LOADER_BINARY}
    COMMAND
    objcopy
        -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc
        --target efi-app-${MOS_ARCH}
        $<TARGET_FILE:mos_uefi_loader>
        ${UEFI_LOADER_BINARY}
    DEPENDS mos_uefi_loader
    COMMENT "Building UEFI bootloader"
)

# add another post build step to generate the EFI file with debug sections (.debug_info .debug_abbrev .debug_loc .debug_aranges .debug_line .debug_macinfo .debug_str)
add_custom_command(TARGET mos_uefi_loader
    POST_BUILD
    BYPRODUCTS ${UEFI_LOADER_BINARY}.debug
    COMMAND
    objcopy
        -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc
        -j .debug_info -j .debug_abbrev -j .debug_loc -j .debug_loclists -j .debug_rnglists -j .debug_aranges -j .debug_line -j .debug_macinfo -j .debug_str -j .debug_line_str
        -j .symtab -j .strtab -j .shstrtab
        --target efi-app-${MOS_ARCH}
        $<TARGET_FILE:mos_uefi_loader>
        ${UEFI_LOADER_BINARY}.debug
    DEPENDS mos_uefi_loader
    COMMENT "Building UEFI bootloader with debug sections"
)

add_summary_item(BOOTABLE mos_uefi_loader.efi ${UEFI_LOADER_BINARY} "EFI Bootloader")
add_summary_item(BOOTABLE mos_uefi_loader.efi.debug ${UEFI_LOADER_BINARY}.debug "EFI Bootloader Debug Symbols")
