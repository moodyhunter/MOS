# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_SYSTEM_NAME Linux) # "MOS" ?

set(CMAKE_C_FLAGS "-Werror=div-by-zero -Wstrict-prototypes")
set(CMAKE_CXX_FLAGS "-Werror=div-by-zero -Wstrict-prototypes")

set(CMAKE_C_FLAGS_DEBUG "-ggdb3")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")

set(CMAKE_C_COMPILER_LAUNCHER "")
set(CMAKE_CXX_COMPILER_LAUNCHER "")

macro(mos_target_setup ISA_FAMILY ISA BITS)
    mos_kconfig(KERNEL MOS_BITS ${BITS} "ISA Bits")
    set(MOS_ISA_FAMILY ${ISA_FAMILY})

    set(CMAKE_SYSTEM_PROCESSOR ${ISA})

    # set(CMAKE_ASM_NASM_OBJECT_FORMAT elf${BITS})
    # Add debug info to nasm
    set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> -felf${BITS} -gdwarf -o <OBJECT> <SOURCE>")

    find_program(CC_PATH NAMES "${ISA}-elf-gcc" NO_CACHE)

    if(NOT CC_PATH)
        message(FATAL_ERROR "TOOLCHAIN: Could not find a C compiler for ${ISA}")
    endif()

    find_program(CXX_PATH NAMES "${ISA}-elf-g++" NO_CACHE)

    if(NOT CXX_PATH)
        message(FATAL_ERROR "TOOLCHAIN: Could not find a C++ compiler for ${ISA}")
    endif()

    set(CMAKE_C_COMPILER ${CC_PATH})
    set(CMAKE_CXX_COMPILER ${CXX_PATH})

    execute_process(COMMAND ${CC_PATH} ${CMAKE_C_FLAGS} -print-file-name=crtbegin.o
        OUTPUT_VARIABLE MOS_CRTBEGIN
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${CC_PATH} ${CMAKE_C_FLAGS} -print-file-name=crtend.o
        OUTPUT_VARIABLE MOS_CRTEND
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    set(CMAKE_C_COMPILER_WORKS 1)
    set(CMAKE_CXX_COMPILER_WORKS 1)
    message(STATUS "${ISA}: C compiler: ${CC_PATH}")
    message(STATUS "${ISA}: C++ compiler: ${CXX_PATH}")
    message(STATUS "${ISA}: CRTBEGIN: ${MOS_CRTBEGIN}, CRTEND: ${MOS_CRTEND}")

    # to be used later?
    add_library(mos::crtbegin IMPORTED STATIC GLOBAL)
    set_target_properties(mos::crtbegin PROPERTIES IMPORTED_LOCATION ${MOS_CRTBEGIN})

    add_library(mos::crtend IMPORTED STATIC GLOBAL)
    set_target_properties(mos::crtend PROPERTIES IMPORTED_LOCATION ${MOS_CRTEND})
endmacro()
