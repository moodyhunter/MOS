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

set(INITRD_DIR "${CMAKE_BINARY_DIR}/initrd")
make_directory(${INITRD_DIR})

add_custom_target(mos_initrd
    find . -depth | sort | cpio -o --format=crc >../initrd.cpio
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/initrd
    COMMENT "Creating initrd at ${CMAKE_BINARY_DIR}/initrd.cpio"
    BYPRODUCTS ${CMAKE_BINARY_DIR}/initrd.cpio
)
add_summary_item(UTILITY mos_initrd "${CMAKE_BINARY_DIR}/initrd.cpio" "Create Initrd")

add_custom_target(mos_cleanup_initrd
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/initrd
    COMMENT "Cleaning up initrd"
)
add_summary_item(UTILITY mos_cleanup_initrd "" "Cleanup Initrd")

macro(add_to_initrd ITEM_TYPE SOURCE_ITEM PATH)
    set(OUTPUT_DIR "${INITRD_DIR}${PATH}")

    # append a slash to the path if there's none
    string(REGEX REPLACE "(.*[^\\/])$" "\\1/" OUTPUT_DIR_PRETTY "${PATH}")

    if("${ITEM_TYPE}" STREQUAL "TARGET")
        set(SOURCE_ITEM_SUPPLIMENTARY_INFO "${SOURCE_ITEM}")
        set(OUTPUT_DIR_PRETTY "${OUTPUT_DIR_PRETTY}${SOURCE_ITEM}")

        set(TARGET_NAME _mos_initrd_target_${SOURCE_ITEM})
        add_custom_target(${TARGET_NAME}
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${SOURCE_ITEM}> ${OUTPUT_DIR}
            COMMENT "Copying target ${SOURCE_ITEM} to initrd"
            DEPENDS ${SOURCE_ITEM} mos_cleanup_initrd
            BYPRODUCTS ${OUTPUT_DIR}/${SOURCE_ITEM}
        )
    else()
        # replace files and directories with their full path, then replace with a pretty prefix
        file(REAL_PATH ${SOURCE_ITEM} SOURCE_ITEM_FULL BASE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

        string(REPLACE "${CMAKE_BINARY_DIR}/"           "@build/"   SOURCE_ITEM_PRETTY "${SOURCE_ITEM_FULL}")
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/"   "@/"    SOURCE_ITEM_PRETTY "${SOURCE_ITEM_PRETTY}")
        string(REPLACE "${CMAKE_SOURCE_DIR}/"           "#/"     SOURCE_ITEM_PRETTY "${SOURCE_ITEM_PRETTY}")
        set(SOURCE_ITEM_SUPPLIMENTARY_INFO "${SOURCE_ITEM_PRETTY}")

        if("${ITEM_TYPE}" STREQUAL "FILE")
            get_filename_component(FILE_NAME ${SOURCE_ITEM_FULL} NAME)
            set(OUTPUT_DIR_PRETTY "${OUTPUT_DIR_PRETTY}${FILE_NAME}")

            set(TARGET_NAME _mos_initrd_file_${SOURCE_ITEM_FULL})
            string(REPLACE "/" "_" TARGET_NAME ${TARGET_NAME})
            add_custom_target(${TARGET_NAME}
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_ITEM_FULL} ${OUTPUT_DIR}
                COMMENT "Copying file ${SOURCE_ITEM_PRETTY} to initrd"
                DEPENDS ${SOURCE_ITEM_FULL} mos_cleanup_initrd
                BYPRODUCTS ${OUTPUT_DIR}/${FILE_NAME}
            )
        elseif("${ITEM_TYPE}" STREQUAL "DIRECTORY")
            set(SOURCE_DIR_FULL ${CMAKE_CURRENT_LIST_DIR}/${SOURCE_ITEM})
            file(GLOB ALL_FILES RELATIVE ${SOURCE_DIR_FULL} "${SOURCE_DIR_FULL}/*")

            # create a list of all files in the target directory
            set(DEST_FILES "") # the files to be used by the BYPRODUCTS property
            set(FILES) # the files to be used by the configuration summary
            foreach(FILE ${ALL_FILES})
                list(APPEND DEST_FILES "${OUTPUT_DIR}/${FILE}")
                list(APPEND FILES "${FILE}")
            endforeach()
            list(JOIN FILES ", " PRETTY_FILES)
            set(SOURCE_ITEM_SUPPLIMENTARY_INFO "${SOURCE_ITEM_SUPPLIMENTARY_INFO}/{${PRETTY_FILES}}")

            set(TARGET_NAME _mos_initrd_directory_${SOURCE_ITEM}_to_${PATH})
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
    endif()

    add_dependencies(mos_initrd ${TARGET_NAME})
    add_summary_item(INITRD "${ITEM_TYPE}" "${SOURCE_ITEM_SUPPLIMENTARY_INFO}" "${OUTPUT_DIR_PRETTY}")
endmacro()

add_custom_target(mos_userspace_programs)
add_summary_item(UTILITY mos_userspace_programs "" "All Userspace Programs")

macro(setup_userspace_program TARGET INITRD_PATH DESCRIPTION)
    add_dependencies(mos_userspace_programs ${TARGET})

    set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(${TARGET} PRIVATE mos::libuserspace)

    add_to_initrd(TARGET ${TARGET} ${INITRD_PATH})
    add_summary_item(USERSPACE "${TARGET}" "${INITRD_PATH}/${TARGET}" "${DESCRIPTION}")
endmacro()
