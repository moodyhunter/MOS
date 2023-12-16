# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT DEFINED MOS_ARCH)
    message(FATAL_ERROR "MOS_ARCH is not defined")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/toolchains/${MOS_ARCH}-mos.cmake)

set(_ARCH_CONFIGURATION_FILE ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH}/platform_config.cmake)
set(_ARCH_LISTS ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH}/CMakeLists.txt)

if(NOT EXISTS ${_ARCH_CONFIGURATION_FILE} OR NOT EXISTS ${_ARCH_LISTS})
    message(FATAL_ERROR "Architecture '${MOS_ARCH}' is not supported, or the configuration file is missing.")
endif()

include(${_ARCH_CONFIGURATION_FILE})

if(NOT DEFINED MOS_BITS)
    message(FATAL_ERROR "MOS_BITS is not defined, the target architecture config file is not correct")
endif()

set(MOS_CX_FLAGS "${MOS_CX_FLAGS} -Wall -Wextra -Wpedantic -pedantic -Werror=div-by-zero")

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${MOS_CX_FLAGS} -Wstrict-prototypes -Wold-style-definition")
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
endif()

message(STATUS "Target: C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Target: C++ compiler: ${CMAKE_CXX_COMPILER}")
