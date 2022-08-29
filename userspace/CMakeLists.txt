# SPDX-License-Identifier: GPL-3.0-or-later

mos_add_summary_section(USERSPACE "Userspace Programs")

add_library(mos_userspace_lib INTERFACE)
add_library(mos::userspace ALIAS mos_userspace_lib)
target_include_directories(mos_userspace_lib
    INTERFACE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_SOURCE_DIR}/arch/${MOS_TARGET_ARCH}/include
)

macro(add_userspace_program NAME)
    set(OPTIONS "")
    set(ONE_VALUE_ARGS "INSTALL_PREFIX")
    set(MULTI_VALUE_ARGS "SOURCES")
    cmake_parse_arguments(ARG "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})
    if (ARG_UNPARSED_ARGUEMENTS)
        message(FATAL_ERROR "Unknown argument(s): ${ARG_UNPARSED_ARGUEMENTS}")
    endif()

    add_executable(${NAME} ${ARG_SOURCES})
    set_target_properties(${NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${ARG_INSTALL_PREFIX}")
    target_link_libraries(${NAME} PUBLIC mos::userspace)
    mos_add_summary_item(USERSPACE ${NAME} ${ARG_INSTALL_PREFIX}/${NAME})
endmacro()

add_userspace_program(init SOURCES init/main.c INSTALL_PREFIX /initrd/bin)
