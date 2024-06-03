# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT DEFINED MOS_ARCH)
    message(FATAL_ERROR "MOS_ARCH is not defined")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/toolchains/${MOS_ARCH}-mos.cmake)
set(CMAKE_PREFIX_PATH) # we are only cross-compiling

set(_ARCH_CONFIGURATION_FILE ${CMAKE_SOURCE_DIR}/kernel/arch/${MOS_ARCH}/platform_config.cmake)
set(_ARCH_LISTS ${CMAKE_SOURCE_DIR}/kernel/arch/${MOS_ARCH}/CMakeLists.txt)

if(NOT EXISTS ${_ARCH_CONFIGURATION_FILE} OR NOT EXISTS ${_ARCH_LISTS})
    message(FATAL_ERROR "Architecture '${MOS_ARCH}' is not supported, or the configuration file is missing.")
endif()

include(${_ARCH_CONFIGURATION_FILE})

list(APPEND MOS_KERNEL_CFLAGS "-ffreestanding")
list(APPEND MOS_KERNEL_CXXFLAGS "-ffreestanding;-fno-exceptions;-fno-rtti")

# global compiler flags, which should be used for all targets
set(MOS_GLOBAL_C_CXX_FLAGS "${MOS_GLOBAL_C_CXX_FLAGS} -Wall -Wextra -Wpedantic -pedantic -Werror=div-by-zero")

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${MOS_GLOBAL_C_CXX_FLAGS} -Wstrict-prototypes -Wold-style-definition -Werror=implicit-function-declaration")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MOS_GLOBAL_C_CXX_FLAGS}")

set(CMAKE_C_FLAGS_DEBUG "-ggdb3")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")

set(CMAKE_C_COMPILER_LAUNCHER "")
set(CMAKE_CXX_COMPILER_LAUNCHER "")

# set(CMAKE_ASM_NASM_OBJECT_FORMAT elf${BITS})
# Add debug info to nasm
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> -felf64 -gdwarf -o <OBJECT> <SOURCE>")

if (__MOS_HAS_NO_COMPILER)
    set(CMAKE_C_COMPILER "true")
    set(CMAKE_CXX_COMPILER "true")
    set(CMAKE_ASM_NASM_COMPILER "true")
    set(CMAKE_ASM_COMPILER "true")
    set(CMAKE_C_COMPILER_WORKS 1)
    set(CMAKE_CXX_COMPILER_WORKS 1)
    set(CMAKE_ASM_NASM_COMPILER_WORKS 1)
    set(CMAKE_ASM_COMPILER_WORKS 1)
    message(WARNING "__MOS_HAS_NO_COMPILER is set, using true as compiler.")
endif()

message(STATUS "Target: C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Target: C++ compiler: ${CMAKE_CXX_COMPILER}")
