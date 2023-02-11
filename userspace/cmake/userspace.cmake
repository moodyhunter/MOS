# SPDX-License-Identifier: GPL-3.0-or-later

# create a gdbinit file for debugging
# ! GDB doesn't pick correct symbol file when debugging multiple processes, solve this.
write_file(${CMAKE_BINARY_DIR}/gdbinit "# GDB init file for MOS")
write_file(${CMAKE_BINARY_DIR}/gdbinit "" APPEND)

macro(add_to_gdbinit TARGET)
    get_target_property(OUT_DIR ${TARGET} RUNTIME_OUTPUT_DIRECTORY)
    file(APPEND ${CMAKE_BINARY_DIR}/gdbinit "\r\n# ${TARGET}\r\n")
    file(APPEND ${CMAKE_BINARY_DIR}/gdbinit "add-symbol-file ${OUT_DIR}/${TARGET}")
endmacro()

add_custom_target(mos_initrd
    find . -depth | cpio -o --format=crc >../initrd.cpio
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/initrd
    COMMENT "Creating initrd at ${CMAKE_BINARY_DIR}/initrd.cpio"
    BYPRODUCTS ${CMAKE_BINARY_DIR}/initrd.cpio
)

add_custom_target(mos_cleanup_initrd
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/initrd
    COMMENT "Cleaning up initrd"
)

macro(add_to_initrd ITEM_TYPE SOURCE_ITEM PATH)
    set(INITRD_DIR "${CMAKE_BINARY_DIR}/initrd")
    set(OUTPUT_DIR "${INITRD_DIR}${PATH}")

    message(STATUS "Adding ${ITEM_TYPE} ${SOURCE_ITEM} to initrd at ${PATH}")

    if("${ITEM_TYPE}" STREQUAL "TARGET")
        set(TARGET_NAME _mos_initrd_target_${SOURCE_ITEM})

        add_custom_target(${TARGET_NAME}
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${SOURCE_ITEM}> ${OUTPUT_DIR}
            COMMENT "Copying target ${SOURCE_ITEM} to initrd"
            DEPENDS ${SOURCE_ITEM} mos_cleanup_initrd
            BYPRODUCTS ${OUTPUT_DIR}/${SOURCE_ITEM}
        )
    elseif("${ITEM_TYPE}" STREQUAL "FILE")
        set(SOURCE_FILE ${CMAKE_CURRENT_LIST_DIR}/${SOURCE_ITEM})
        set(TARGET_NAME _mos_initrd_file_${SOURCE_FILE})
        string(REPLACE "/" "_" TARGET_NAME ${TARGET_NAME})

        get_filename_component(FILE_NAME ${SOURCE_FILE} NAME)

        add_custom_target(${TARGET_NAME}
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_FILE} ${OUTPUT_DIR}
            COMMENT "Copying file ${SOURCE_FILE} to initrd"
            DEPENDS ${SOURCE_FILE} mos_cleanup_initrd
            BYPRODUCTS ${OUTPUT_DIR}/${FILE_NAME}
        )
    elseif("${ITEM_TYPE}" STREQUAL "DIRECTORY")
        set(TARGET_NAME _mos_initrd_directory_${SOURCE_ITEM}_to_${PATH})
        set(SOURCE_DIR_FULL ${CMAKE_CURRENT_LIST_DIR}/${SOURCE_ITEM})

        file(GLOB ALL_FILES RELATIVE ${SOURCE_DIR_FULL} "${SOURCE_DIR_FULL}/*")

        # create a list of all files in the target directory
        set(DEST_FILES "")
        foreach(FILE ${ALL_FILES})
            list(APPEND DEST_FILES "${OUTPUT_DIR}/${FILE}")
        endforeach()

        string(REPLACE "/" "_" TARGET_NAME ${TARGET_NAME})
        add_custom_target(${TARGET_NAME}
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            COMMAND cp -rT ${SOURCE_ITEM} ${OUTPUT_DIR} # see https://gitlab.kitware.com/cmake/cmake/-/issues/14609
            COMMENT "Copying directory ${SOURCE_ITEM} to initrd"
            DEPENDS ${SOURCE_ITEM} mos_cleanup_initrd
            BYPRODUCTS ${DEST_FILES}
        )
    else()
        message(FATAL_ERROR "Unknown initrd item type: ${ITEM_TYPE}")
    endif()

    add_dependencies(mos_initrd ${TARGET_NAME})
endmacro()

add_custom_target(mos_userspace_programs)
summary_section(USERSPACE "Userspace Programs")

macro(setup_userspace_program TARGET INITRD_PATH DESCRIPTION)
    add_dependencies(mos_userspace_programs ${TARGET})

    set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(${TARGET} PRIVATE mos::libuserspace)

    add_to_initrd(TARGET ${TARGET} ${INITRD_PATH})
    add_summary_item(USERSPACE "${TARGET}" "${DESCRIPTION}" "${INITRD_PATH}/${TARGET}")
endmacro()
