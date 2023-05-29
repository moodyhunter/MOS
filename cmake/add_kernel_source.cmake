# SPDX-License-Identifier: GPL-3.0-or-later

macro(add_kernel_source)
    set(OPTIONS "")
    set(SINGLE_VALUE_OPTIONS "")
    set(MULTI_VALUE_OPTIONS "SOURCES;INCLUDE_DIRECTORIES;SYSTEM_INCLUDE_DIRECTORIES")
    cmake_parse_arguments(ARG "${OPTIONS}" "${SINGLE_VALUE_OPTIONS}" "${MULTI_VALUE_OPTIONS}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unparsed arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    target_sources(mos_kernel PRIVATE ${ARG_SOURCES})
    target_include_directories(mos_kernel PRIVATE ${ARG_INCLUDE_DIRECTORIES})
    target_include_directories(mos_kernel SYSTEM PRIVATE ${ARG_SYSTEM_INCLUDE_DIRECTORIES})
endmacro()
