# SPDX-License-Identifier: GPL-3.0-or-later

find_program(SYSTEM_GCC gcc REQUIRED)
set(CMAKE_C_COMPILER ${SYSTEM_GCC})

project(mos_uefi_bootloader C ASM)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_library(gnuefi NAMES libgnuefi.a REQUIRED)
find_library(efi NAMES libefi.a REQUIRED)
find_path(gnuefi_include_dir efi.h REQUIRED PATH_SUFFIXES include include/gnuefi include/efi)
find_file(gnuefi_crt0 crt0-efi-${MOS_ARCH}.o REQUIRED PATH_SUFFIXES lib)
find_file(gnuefi_lds elf_${MOS_ARCH}_efi.lds REQUIRED PATH_SUFFIXES lib)

add_library(mos_uefi_loader SHARED unified_bootloader.c)
target_compile_options(mos_uefi_loader PRIVATE -ffreestanding -fshort-wchar -mno-red-zone -ggdb)
target_compile_options(mos_uefi_loader PRIVATE -Wno-strict-prototypes) # GNU-EFI produces warnings
target_link_options(mos_uefi_loader PRIVATE -nostdlib -Wl,-Bsymbolic -znocombreloc -T ${gnuefi_lds})
target_include_directories(mos_uefi_loader PRIVATE ${gnuefi_include_dir})
target_link_libraries(mos_uefi_loader ${gnuefi_crt0} ${gnuefi} ${efi})

add_custom_command(TARGET mos_uefi_loader
    POST_BUILD
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/mos_uefi_loader.efi
    COMMAND
    objcopy
        -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc
        --target efi-app-${MOS_ARCH}
        $<TARGET_FILE:mos_uefi_loader>
        ${CMAKE_CURRENT_BINARY_DIR}/mos_uefi_loader.efi
    DEPENDS mos_uefi_loader
    COMMENT "Building UEFI bootloader"
)

# add another post build step to generate the EFI file with debug sections (.debug_info .debug_abbrev .debug_loc .debug_aranges .debug_line .debug_macinfo .debug_str)
add_custom_command(TARGET mos_uefi_loader
    POST_BUILD
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/mos_uefi_loader.debug
    COMMAND
    objcopy
        -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc
        -j .debug_info -j .debug_abbrev -j .debug_loc -j .debug_loclists -j .debug_rnglists -j .debug_aranges -j .debug_line -j .debug_macinfo -j .debug_str -j .debug_line_str
        -j .symtab -j .strtab -j .shstrtab
        --target efi-app-${MOS_ARCH}
        $<TARGET_FILE:mos_uefi_loader>
        ${CMAKE_CURRENT_BINARY_DIR}/mos_uefi_loader.debug
    DEPENDS mos_uefi_loader
    COMMENT "Building UEFI bootloader with debug sections"
)
