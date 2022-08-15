# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_SYSTEM_NAME Linux)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffreestanding")

set(CMAKE_EXE_LINKER_FLAGS "-nostdlib")
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)

# Add debug info to nasm
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -f ${CMAKE_ASM_NASM_OBJECT_FORMAT} -w+gnu-elf-extensions -F dwarf -g -o <OBJECT> <SOURCE>")

# For the C compiler, 'elf' should be in the OS field for target triplet to produce a bare metal binary.
# Although the `linux-gnu` targeted gcc 'does' work, it's highly unsuggested.
find_program(CC_PATH NAMES "i686-elf-gcc" "i686-linux-gnu-gcc")
if(NOT CC_PATH)
    message(FATAL_ERROR "TOOLCHAIN: Could not find i686 C compiler")
endif()

find_program(CXX_PATH NAMES "i686-elf-g++" "i686-linux-gnu-g++")
if(NOT CXX_PATH)
    message(FATAL_ERROR "TOOLCHAIN: Could not find i686 C++ compiler")
endif()

message(STATUS "TOOLCHAIN: i686 C compiler: ${CC_PATH}")
message(STATUS "TOOLCHAIN: i686 C++ compiler: ${CXX_PATH}")
set(CMAKE_C_COMPILER ${CC_PATH})
set(CMAKE_CXX_COMPILER ${CXX_PATH})
