# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding")

# For the C compiler, 'elf' should be in the OS field for target triplet to produce a bare metal binary.
# Although the `linux-gnu` targeted gcc 'does' work, it's highly unsuggested.

find_program(CC_PATH NAMES "i686-elf-gcc" "i686-linux-gnu-gcc")
if(CC_PATH)
    message(STATUS "TOOLCHAIN: Found i686 gcc: ${CC_PATH}")
    set(CMAKE_C_COMPILER ${CC_PATH})
else()
    message(FATAL_ERROR "TOOLCHAIN: Could not find i686 gcc")
endif()
