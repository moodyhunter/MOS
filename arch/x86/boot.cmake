# SPDX-License-Identifier: GPL-3.0-or-later

if(MOS_BOOT_METHOD STREQUAL "raw")
    include(${CMAKE_CURRENT_LIST_DIR}/raw_boot/raw_boot.cmake)
elseif(MOS_BOOT_METHOD STREQUAL "multiboot")
    include(${CMAKE_CURRENT_LIST_DIR}/multiboot/multiboot.cmake)
endif()
