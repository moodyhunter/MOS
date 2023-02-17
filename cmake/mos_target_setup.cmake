# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_SYSTEM_NAME Linux) # "MOS" ?

set(CMAKE_C_FLAGS "-Werror=div-by-zero -Wstrict-prototypes")
set(CMAKE_CXX_FLAGS "-Werror=div-by-zero -fno-rtti -fno-exceptions")

set(CMAKE_C_FLAGS_DEBUG "-ggdb3")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")

set(CMAKE_C_COMPILER_LAUNCHER "")
set(CMAKE_CXX_COMPILER_LAUNCHER "")

macro(mos_target_setup)
    # set(CMAKE_ASM_NASM_OBJECT_FORMAT elf${BITS})
    # Add debug info to nasm
    set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> -felf${MOS_BITS} -gdwarf -o <OBJECT> <SOURCE>")

    find_program(CMAKE_C_COMPILER NAMES "${MOS_COMPILER_PREFIX}gcc" NO_CACHE REQUIRED)
    find_program(CMAKE_CXX_COMPILER NAMES "${MOS_COMPILER_PREFIX}g++" NO_CACHE REQUIRED)

    execute_process(COMMAND ${CMAKE_C_COMPILER} "-print-file-name=crtbegin.o"
        OUTPUT_VARIABLE MOS_CRTBEGIN
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} "-print-file-name=crtend.o"
        OUTPUT_VARIABLE MOS_CRTEND
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    set(CMAKE_C_COMPILER_WORKS 1)
    set(CMAKE_CXX_COMPILER_WORKS 1)
    message(STATUS "Target: C compiler: ${CMAKE_C_COMPILER}")
    message(STATUS "Target: C++ compiler: ${CMAKE_CXX_COMPILER}")
    message(STATUS "Target: CRTBEGIN: ${MOS_CRTBEGIN}, CRTEND: ${MOS_CRTEND}")

    # to be used later?
    add_library(mos::crtbegin IMPORTED STATIC GLOBAL)
    set_target_properties(mos::crtbegin PROPERTIES IMPORTED_LOCATION ${MOS_CRTBEGIN})

    add_library(mos::crtend IMPORTED STATIC GLOBAL)
    set_target_properties(mos::crtend PROPERTIES IMPORTED_LOCATION ${MOS_CRTEND})
endmacro()
