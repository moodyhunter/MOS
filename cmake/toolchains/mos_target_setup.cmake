# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffreestanding")
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib")
set(CMAKE_SYSTEM_NAME Linux) # "MOS" ?

set(CMAKE_C_FLAGS_DEBUG "-ggdb3")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")

macro(mos_target_setup ISA_FAMILY ISA BITS)
    set(CMAKE_SYSTEM_PROCESSOR ${ISA})
    set(CMAKE_ASM_NASM_OBJECT_FORMAT elf${BITS})
    # Add debug info to nasm
    set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <FLAGS> -f ${CMAKE_ASM_NASM_OBJECT_FORMAT} -gdwarf -o <OBJECT> <SOURCE>")

    find_program(CC_PATH NAMES "${ISA}-elf-gcc" NO_CACHE)
    if(NOT CC_PATH)
        message(FATAL_ERROR "TOOLCHAIN: Could not find a C compiler for ${ISA}")
    endif()

    find_program(CXX_PATH NAMES "${ISA}-elf-g++" NO_CACHE)
    if(NOT CXX_PATH)
        message(FATAL_ERROR "TOOLCHAIN: Could not find a C++ compiler for ${ISA}")
    endif()

    message(STATUS "${ISA}: C compiler: ${CC_PATH}")
    message(STATUS "${ISA}: C++ compiler: ${CXX_PATH}")
    set(CMAKE_C_COMPILER ${CC_PATH})
    set(CMAKE_CXX_COMPILER ${CXX_PATH})
    set(MOS_TARGET_ARCH ${ISA_FAMILY})
endmacro()
