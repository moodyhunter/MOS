# SPDX-License-Identifier: GPL-3.0-or-later

macro(mos_add_bootable FOLDER)
    include(${CMAKE_CURRENT_LIST_DIR}/${FOLDER}/bootable.cmake)
    if(NOT TARGET ${FOLDER})
        message(FATAL_ERROR "Failed to add bootable '${FOLDER}': missing target")
    endif()
    if(NOT MOS_BOOTABLE_TARGETS_FILE_${FOLDER})
        message(FATAL_ERROR "Failed to add bootable '${FOLDER}': missing target file")
    endif()
    list(APPEND MOS_BOOTABLE_TARGETS ${FOLDER})
endmacro()
