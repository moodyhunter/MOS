# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_SYSTEM_NAME Linux) # "MOS" ?

macro(mos_target_setup)
    set(MOS_CX_FLAGS "${MOS_CX_FLAGS} -Wall -Wextra -Wpedantic -pedantic -Werror=div-by-zero")

    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${MOS_CX_FLAGS} -Wstrict-prototypes")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MOS_CX_FLAGS} -fno-rtti -fno-exceptions")

    set(CMAKE_C_FLAGS_DEBUG "-ggdb3")
    set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")

    set(CMAKE_C_COMPILER_LAUNCHER "")
    set(CMAKE_CXX_COMPILER_LAUNCHER "")

    set(CMAKE_C_COMPILER_WORKS 1)
    set(CMAKE_CXX_COMPILER_WORKS 1)

    # set(CMAKE_ASM_NASM_OBJECT_FORMAT elf${BITS})
    # Add debug info to nasm
    set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> -felf${MOS_BITS} -gdwarf -o <OBJECT> <SOURCE>")

    if (__MOS_HAS_NO_COMPILER)
        set(CMAKE_C_COMPILER "true")
        set(CMAKE_CXX_COMPILER "true")
        set(CMAKE_ASM_NASM_COMPILER "true")
        set(CMAKE_ASM_NASM_COMPILER_WORKS 1)
        message(WARNING "__MOS_HAS_NO_COMPILER is set, using true as compiler.")
    else()
        find_program(CMAKE_C_COMPILER NAMES "${MOS_COMPILER_PREFIX}gcc" NO_CACHE)
        find_program(CMAKE_CXX_COMPILER NAMES "${MOS_COMPILER_PREFIX}g++" NO_CACHE)
    endif()

    execute_process(COMMAND ${CMAKE_C_COMPILER} "-print-file-name=crtbegin.o"
        OUTPUT_VARIABLE MOS_CRTBEGIN
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} "-print-file-name=crtend.o"
        OUTPUT_VARIABLE MOS_CRTEND
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    message(STATUS "Target: C compiler: ${CMAKE_C_COMPILER}")
    message(STATUS "Target: C++ compiler: ${CMAKE_CXX_COMPILER}")
    message(STATUS "Target: CRTBEGIN: ${MOS_CRTBEGIN}, CRTEND: ${MOS_CRTEND}")

    # to be used later?
    add_library(mos::crtbegin IMPORTED STATIC GLOBAL)
    set_target_properties(mos::crtbegin PROPERTIES IMPORTED_LOCATION "${MOS_CRTBEGIN}")

    add_library(mos::crtend IMPORTED STATIC GLOBAL)
    set_target_properties(mos::crtend PROPERTIES IMPORTED_LOCATION "${MOS_CRTEND}")
endmacro()
