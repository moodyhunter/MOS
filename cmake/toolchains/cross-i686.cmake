# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_SYSTEM_PROCESSOR i686)
list(APPEND CMAKE_EXE_LINKER_FLAGS "-nostdlib -lgcc")
list(APPEND CMAKE_C_FLAGS "-ffreestanding")

find_program(CC_PATH NAMES "i686-linux-gnu-gcc" "i686-elf-gcc")
if(CC_PATH)
    message(STATUS "TOOLCHAIN: Found i686 gcc: ${CC_PATH}")
    set(CMAKE_C_COMPILER ${CC_PATH})
else()
    message(FATAL_ERROR "TOOLCHAIN: Could not find i686 gcc")
endif()
