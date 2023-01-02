# SPDX-License-Identifier: GPL-3.0-or-later

summary_section(USERSPACE "Userspace Programs")

# TODO: Remove this once we have a proper userspace libc
target_compile_options(mos_libuserspace PUBLIC "-ffreestanding")
target_link_options(mos_libuserspace PUBLIC "-nostdlib")

# create a gdbinit file for debugging
# ! GDB doesn't pick correct symbol file when debugging multiple processes, solve this.
write_file(${CMAKE_BINARY_DIR}/gdbinit "# GDB init file for MOS")
write_file(${CMAKE_BINARY_DIR}/gdbinit "" APPEND)

add_custom_target(mos_userspace_programs)

macro(add_userspace_program NAME)
    set(OPTIONS "")
    set(ONE_VALUE_ARGS "INSTALL_PREFIX")
    set(MULTI_VALUE_ARGS "SOURCES")
    cmake_parse_arguments(ARG "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})
    if (ARG_UNPARSED_ARGUEMENTS)
        message(FATAL_ERROR "Unknown argument(s): ${ARG_UNPARSED_ARGUEMENTS}")
    endif()

    add_executable(${NAME} EXCLUDE_FROM_ALL ${ARG_SOURCES})

    get_target_property(BINARY_OUTPUT_DIR ${NAME} RUNTIME_OUTPUT_DIRECTORY)

    if(ARG_INSTALL_PREFIX)
        set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/${ARG_INSTALL_PREFIX}")
    else()
        set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    set_target_properties(${NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
    target_link_libraries(${NAME} PUBLIC mos::libuserspace)
    mos_add_summary_item(USERSPACE ${NAME} "Userspace Program ${NAME}" ${OUTPUT_DIR}/${NAME})
    write_file(${CMAKE_BINARY_DIR}/gdbinit "add-symbol-file ${OUTPUT_DIR}/${NAME}" APPEND)
    add_dependencies(mos_userspace_programs ${NAME})
endmacro()

add_custom_target(mos_initrd
    find . -depth | cpio -o --format=crc >../initrd.cpio
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/initrd
    COMMENT "Creating initrd at ${CMAKE_BINARY_DIR}/initrd.cpio"
    BYPRODUCTS ${CMAKE_BINARY_DIR}/initrd.cpio
)

macro(add_initrd_item ITEM_TYPE PATH SOURCE_NAME)
    set(INITRD_DIR "${CMAKE_BINARY_DIR}/initrd")
    set(OUTPUT_DIR "${INITRD_DIR}${PATH}")

    message(STATUS "Adding ${ITEM_TYPE} ${SOURCE_NAME} to initrd at ${PATH}")

    if("${ITEM_TYPE}" STREQUAL "TARGET")
        add_dependencies(mos_initrd ${SOURCE_NAME})
        add_custom_command(TARGET ${SOURCE_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${SOURCE_NAME}> ${OUTPUT_DIR}/${SOURCE_NAME}
            COMMENT "Copying ${SOURCE_NAME} to initrd"
            BYPRODUCTS ${OUTPUT_DIR}/${SOURCE_NAME}
        )
    elseif("${ITEM_TYPE}" STREQUAL "FILE")
        set(SOURCE_NAME ${CMAKE_CURRENT_LIST_DIR}/${SOURCE_NAME})
        get_filename_component(FILE_NAME ${SOURCE_NAME} NAME)
        string(REPLACE "/" "_" TARGET_NAME ${SOURCE_NAME})
        add_custom_target(${TARGET_NAME}_initrd
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_NAME} ${OUTPUT_DIR}
            COMMENT "Copying ${SOURCE_NAME} to initrd"
            DEPENDS ${SOURCE_NAME}
            BYPRODUCTS ${OUTPUT_DIR}/${FILE_NAME}
        )
        add_dependencies(mos_initrd ${TARGET_NAME}_initrd)
    else()
        message(FATAL_ERROR "Unknown initrd item type: ${ITEM_TYPE}")
    endif()
endmacro()

add_userspace_program(init SOURCES init/main.c)
add_userspace_program(kmsg-ping SOURCES ping-pong/ping.c)
add_userspace_program(kmsg-pong SOURCES ping-pong/pong.c)
add_userspace_program(locks SOURCES locks/main.c)

add_initrd_item(TARGET /programs init)
add_initrd_item(TARGET /programs kmsg-ping)
add_initrd_item(TARGET /programs kmsg-pong)
add_initrd_item(TARGET /programs locks)

add_initrd_item(FILE /assets assets/msg.txt)
