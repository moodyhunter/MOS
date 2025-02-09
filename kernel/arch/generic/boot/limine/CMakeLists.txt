# SPDX-License-Identifier: GPL-3.0-or-later

create_bootable_kernel_binary(
    TARGET
        mos_limine
    LINKER_SCRIPT
        limine.ld
    FILENAME
        mos_limine.elf
    SOURCES
        limine.cpp
)

set(LIMINE_ROOT "/usr/share/limine" CACHE PATH "Path to limine root directory")

set(EFI_PART_ROOT "${CMAKE_BINARY_DIR}/uefi-files")

set(LIMINE_CONFIG "
timeout: 0

/limine MOS-${MOS_ARCH}
    protocol: limine
    kernel_path: boot():/boot/MOS/${MOS_ARCH}-kernel.elf
    module_path: boot():/boot/MOS/${MOS_ARCH}-initrd.cpio
")

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/limine.conf ${LIMINE_CONFIG})

# Make UEFI partition files structure
add_custom_target(mos_uefi_partition
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EFI_PART_ROOT}/EFI/BOOT
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EFI_PART_ROOT}/boot/MOS
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EFI_PART_ROOT}/boot/limine

    # copy limine configuration and binaries
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/limine.conf ${EFI_PART_ROOT}/boot/limine/limine.conf
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_ROOT}/limine-uefi-cd.bin       ${EFI_PART_ROOT}/boot/limine/limine-uefi-cd.bin

    # copy limine bootloader
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_ROOT}/BOOTX64.EFI     ${EFI_PART_ROOT}/EFI/BOOT/BOOTX64.EFI || true
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_ROOT}/BOOTRISCV64.EFI ${EFI_PART_ROOT}/EFI/BOOT/BOOTRISCV64.EFI || true
    # copy kernel and initrd
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/boot.dir/mos_limine.elf    ${EFI_PART_ROOT}/boot/MOS/${MOS_ARCH}-kernel.elf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/initrd.cpio                ${EFI_PART_ROOT}/boot/MOS/${MOS_ARCH}-initrd.cpio
    DEPENDS mos_limine mos_initrd
    VERBATIM
)
