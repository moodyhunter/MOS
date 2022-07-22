# SPDX-License-Identifier: GPL-3.0-or-later

include(add_nasm_binary)
include(prepare_bootable_kernel_binary)

mos_add_summary_section(BOOTABLE "Bootable Targets")

set(MOS_BOOTABLE_TARGETS)
set(MOS_BOOTABLE_TARGETS_FILE)

macro(mos_add_bootable FOLDER)
    include(${CMAKE_CURRENT_LIST_DIR}/${FOLDER}/bootable.cmake)
    if(NOT TARGET ${FOLDER})
        message(FATAL_ERROR "Failed to add bootable '${FOLDER}': missing target")
    endif()
    if(NOT MOS_BOOTABLE_TARGETS_FILE_${FOLDER})
        message(FATAL_ERROR "Failed to add bootable '${FOLDER}': missing target file")
    endif()
    mos_add_summary_item(BOOTABLE ${FOLDER} ${MOS_BOOTABLE_TARGETS_FILE_${FOLDER}})
    list(APPEND MOS_BOOTABLE_TARGETS ${FOLDER})
endmacro()

mos_add_bootable(multiboot)
mos_add_bootable(multiboot_iso)

# dump the bootable targets into bootables.json
set(BOOTABLE_DATA "{}")
foreach(target ${MOS_BOOTABLE_TARGETS})
    string(JSON BOOTABLE_DATA SET ${BOOTABLE_DATA} "${target}" \"${MOS_BOOTABLE_TARGETS_FILE_${target}}\")
endforeach()
file(WRITE ${CMAKE_BINARY_DIR}/bootables.json "${BOOTABLE_DATA}")

mos_add_summary_section(BOOTABLE_INFO "Bootable Info File")
mos_add_summary_item(BOOTABLE_INFO "File Location" ${CMAKE_BINARY_DIR}/bootables.json)
