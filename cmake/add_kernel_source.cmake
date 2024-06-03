# SPDX-License-Identifier: GPL-3.0-or-later

function(add_kernel_source)
    set(OPTIONS "")
    set(SINGLE_VALUE_OPTIONS "")
    set(MULTI_VALUE_OPTIONS "INCLUDE_DIRECTORIES;SYSTEM_INCLUDE_DIRECTORIES;PUBLIC_INCLUDE_DIRECTORIES")
    cmake_parse_arguments(ARG "${OPTIONS}" "${SINGLE_VALUE_OPTIONS}" "${MULTI_VALUE_OPTIONS}" ${ARGN})

    target_sources(mos_kernel PRIVATE ${ARG_UNPARSED_ARGUMENTS})
    target_include_directories(mos_kernel PRIVATE ${ARG_INCLUDE_DIRECTORIES})
    target_include_directories(mos_kernel SYSTEM PRIVATE ${ARG_SYSTEM_INCLUDE_DIRECTORIES})
    target_include_directories(mos_kernel PUBLIC ${ARG_PUBLIC_INCLUDE_DIRECTORIES})
endfunction()
