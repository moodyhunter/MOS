# SPDX-License-Identifier: GPL-3.0-or-later

function(output_bootables)
    set(BOOTABLE_FILE_DATA "{}")
    message("  Bootable targets: ")
    foreach(target ${MOS_BOOTABLE_TARGETS})
        string(JSON BOOTABLE_FILE_DATA SET ${BOOTABLE_FILE_DATA} "${target}" \"${MOS_BOOTABLE_TARGETS_FILE_${target}}\")
        message("    - ${target}: ${MOS_BOOTABLE_TARGETS_FILE_${target}}")
    endforeach()
    message("  Bootable targets file: ${CMAKE_BINARY_DIR}/bootables.json")
    message("")
    file(WRITE ${CMAKE_BINARY_DIR}/bootables.json "${BOOTABLE_FILE_DATA}")
endfunction()
