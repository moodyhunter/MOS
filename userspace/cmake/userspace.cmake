# SPDX-License-Identifier: GPL-3.0-or-later

# create a gdbinit file for debugging
# ! GDB doesn't pick correct symbol file when debugging multiple processes, solve this.
write_file(${CMAKE_BINARY_DIR}/gdbinit "# GDB init file for MOS")
write_file(${CMAKE_BINARY_DIR}/gdbinit "" APPEND)

add_custom_target(mos_userspace_programs)

summary_section(USERSPACE "Userspace Programs")

macro(add_to_gdbinit TARGET)
    get_target_property(OUT_DIR ${TARGET} RUNTIME_OUTPUT_DIRECTORY)
    file(APPEND ${CMAKE_BINARY_DIR}/gdbinit "\r\n# ${TARGET}\r\n")
    file(APPEND ${CMAKE_BINARY_DIR}/gdbinit "add-symbol-file ${OUT_DIR}/${TARGET}")
endmacro()

add_custom_target(mos_cleanup_initrd
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/initrd
    COMMENT "Cleaning up initrd"
)

macro(add_to_initrd ITEM_TYPE SOURCE_NAME PATH)
    set(INITRD_DIR "${CMAKE_BINARY_DIR}/initrd")
    set(OUTPUT_DIR "${INITRD_DIR}${PATH}")

    message(STATUS "Adding ${ITEM_TYPE} ${SOURCE_NAME} to initrd at ${PATH}")

    if("${ITEM_TYPE}" STREQUAL "TARGET")
        add_custom_target(${SOURCE_NAME}_initrd
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${SOURCE_NAME}> ${OUTPUT_DIR}
            COMMENT "Copying ${SOURCE_NAME} to initrd"
            DEPENDS ${SOURCE_NAME} mos_cleanup_initrd
            BYPRODUCTS ${OUTPUT_DIR}/${SOURCE_NAME}
        )
        add_dependencies(mos_initrd ${SOURCE_NAME}_initrd)
    elseif("${ITEM_TYPE}" STREQUAL "FILE")
        set(SOURCE_NAME ${CMAKE_CURRENT_LIST_DIR}/${SOURCE_NAME})
        get_filename_component(FILE_NAME ${SOURCE_NAME} NAME)
        string(REPLACE "/" "_" TARGET_NAME ${SOURCE_NAME})
        add_custom_target(${TARGET_NAME}_initrd
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_NAME} ${OUTPUT_DIR}
            COMMENT "Copying ${SOURCE_NAME} to initrd"
            DEPENDS ${SOURCE_NAME} mos_cleanup_initrd
            BYPRODUCTS ${OUTPUT_DIR}/${FILE_NAME}
        )
        add_dependencies(mos_initrd ${TARGET_NAME}_initrd)
    elseif("${ITEM_TYPE}" STREQUAL "DIRECTORY")
        set(SOURCE_NAME ${CMAKE_CURRENT_LIST_DIR}/${SOURCE_NAME})
        get_filename_component(DIR_NAME ${SOURCE_NAME} NAME)
        string(REPLACE "/" "_" TARGET_NAME ${SOURCE_NAME})
        add_custom_target(${TARGET_NAME}_initrd
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${SOURCE_NAME} ${OUTPUT_DIR}
            COMMENT "Copying ${SOURCE_NAME} to initrd"
            DEPENDS ${SOURCE_NAME} mos_cleanup_initrd
            BYPRODUCTS ${OUTPUT_DIR}/${DIR_NAME}
        )
        add_dependencies(mos_initrd ${TARGET_NAME}_initrd)
    else()
        message(FATAL_ERROR "Unknown initrd item type: ${ITEM_TYPE}")
    endif()
endmacro()

macro(setup_userspace_program TARGET INITRD_PATH DESCRIPTION)
    add_dependencies(mos_userspace_programs ${TARGET})

    set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(${TARGET} PRIVATE mos::libuserspace)

    add_to_initrd(TARGET ${TARGET} ${INITRD_PATH})
    add_summary_item(USERSPACE "${TARGET}" "${DESCRIPTION}" "${INITRD_PATH}/${TARGET}")
endmacro()
