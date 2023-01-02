# SPDX-License-Identifier: GPL-3.0-or-later

summary_section(BOOTABLE "Bootable Targets")

set(MOS_BOOTABLE_TARGETS)
set(MOS_BOOTABLE_TARGETS_FILE)

macro(add_bootable_target FOLDER)
    set(path ${FOLDER})
    cmake_path(GET path STEM boot_target)
    include(${CMAKE_CURRENT_LIST_DIR}/${FOLDER}/bootable.cmake)

    if(NOT TARGET ${boot_target})
        message(FATAL_ERROR "Failed to add bootable '${boot_target}': missing target")
    endif()

    if(NOT MOS_BOOTABLE_TARGETS_FILE_${boot_target})
        message(FATAL_ERROR "Failed to add bootable '${boot_target}': missing target file")
    endif()

    mos_add_summary_item(BOOTABLE ${boot_target} "Bootable Target" ${MOS_BOOTABLE_TARGETS_FILE_${boot_target}})
    list(APPEND MOS_BOOTABLE_TARGETS ${boot_target})
endmacro()

# dump the bootable targets into bootables.json
macro(dump_bootable_targets)
    set(BOOTABLE_DATA "{}")

    foreach(target ${MOS_BOOTABLE_TARGETS})
        string(JSON BOOTABLE_DATA SET ${BOOTABLE_DATA} "${target}" \"${MOS_BOOTABLE_TARGETS_FILE_${target}}\")
    endforeach()

    file(WRITE ${CMAKE_BINARY_DIR}/bootables.json "${BOOTABLE_DATA}")
endmacro()
