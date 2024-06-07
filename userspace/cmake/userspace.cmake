# SPDX-License-Identifier: GPL-3.0-or-later

# create a gdbinit file for debugging
# ! GDB doesn't pick correct symbol file when debugging multiple processes, solve this.
write_file(${CMAKE_BINARY_DIR}/gdbinit "# GDB init file for MOS")
write_file(${CMAKE_BINARY_DIR}/gdbinit "" APPEND)

macro(add_to_gdbinit_raw LINE)
    file(APPEND ${CMAKE_BINARY_DIR}/gdbinit "${LINE}\r\n")
endmacro()

set(INITRD_DIR "${CMAKE_BINARY_DIR}/initrd" CACHE PATH "The directory to store the initrd in" FORCE)
make_directory(${INITRD_DIR})

add_custom_target(mos_initrd
    find . -depth | sort | cpio --quiet -o --format=crc >../initrd.cpio
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/initrd
    COMMENT "Creating initrd at ${CMAKE_BINARY_DIR}/initrd.cpio"
    BYPRODUCTS ${CMAKE_BINARY_DIR}/initrd.cpio
)
add_summary_item(UTILITY mos_initrd "${CMAKE_BINARY_DIR}/initrd.cpio" "Create Initrd")

add_custom_target(mos_cleanup_initrd
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${INITRD_DIR} && ${CMAKE_COMMAND} -E make_directory ${INITRD_DIR}
    COMMENT "Cleaning up initrd"
)
add_summary_item(UTILITY mos_cleanup_initrd "" "Cleanup Initrd")

macro(add_to_initrd ITEM_TYPE SOURCE_ITEM PATH)
    set(OUTPUT_DIR "${INITRD_DIR}${PATH}/")
    make_directory(${OUTPUT_DIR})

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
            DEPENDS ${SOURCE_ITEM}
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
            get_filename_component(FILE_NAME ${SOURCE_ITEM} NAME)
            set(OUTPUT_DIR_PRETTY "${OUTPUT_DIR_PRETTY}${FILE_NAME}")

            set(TARGET_NAME _mos_initrd_file_${SOURCE_ITEM})
            string(REPLACE "/" "_" TARGET_NAME ${TARGET_NAME})
            add_custom_target(${TARGET_NAME}
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                COMMAND cp --remove-destination --preserve=mode,timestamps -d -r ${SOURCE_ITEM} ${OUTPUT_DIR} || true
                VERBATIM
                COMMENT "Copying file ${SOURCE_ITEM} to initrd"
                DEPENDS ${SOURCE_ITEM}
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
                COMMAND cp --remove-destination -rT ${SOURCE_ITEM} ${OUTPUT_DIR} # see https://gitlab.kitware.com/cmake/cmake/-/issues/14609
                COMMENT "Copying directory ${SOURCE_ITEM} to initrd"
                DEPENDS ${SOURCE_ITEM}
                BYPRODUCTS ${DEST_FILES}
            )
        else()
            message(FATAL_ERROR "Unknown initrd item type: ${ITEM_TYPE}")
        endif()
    endif()

    add_dependencies(mos_initrd ${TARGET_NAME})
endmacro()

if (_MOS_NO_RUST_TARGETS)
    macro(add_simple_rust_project PROJECT_DIR NAME INITRD_SUBDIR)
        message(WARNING "Rust targets are disabled, skipping ${NAME}")
    endmacro()
    return()
endif()

find_program(RUSTC rustc)
if (RUSTC)
    # get rustc --print target-list output
    execute_process(
        COMMAND ${RUSTC} --print target-list
        OUTPUT_VARIABLE RUST_TARGETS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE RUST_TARGETS_RESULT
    )

    if(NOT RUST_TARGETS_RESULT EQUAL 0)
        message(WARNING "Failed to get rustc target list")
        set(RUSTC_IS_SUPPORTED FALSE)
    else()
        # split the output into a list
        string(REPLACE "\n" ";" RUST_TARGETS ${RUST_TARGETS})

        # find the first target that matches the current architecture
        set(RUSTC_IS_SUPPORTED FALSE)
        foreach(TARGET ${RUST_TARGETS})
            if("${TARGET}" STREQUAL "${MOS_RUST_TARGET}")
                set(RUSTC_IS_SUPPORTED TRUE)
                message(STATUS "Found rustc target ${MOS_RUST_TARGET}")
                break()
            endif()
        endforeach()
    endif()
else()
    message(WARNING "rustc not found, rust targets will not be supported")
endif()

macro(add_simple_rust_project PROJECT_DIR NAME INITRD_SUBDIR)
    if (NOT RUSTC_IS_SUPPORTED)
        message(WARNING "Rust target ${MOS_RUST_TARGET} is not supported by rustc, skipping ${NAME}")
        return()
    endif()

    cmake_parse_arguments(ARGS "" "" "DEPENDS" ${ARGN})

    set(OUTPUT_FILE "${PROJECT_DIR}/target/${MOS_RUST_TARGET}/debug/${NAME}")
    add_custom_target(
        ${NAME}_rust
        cargo build --quiet --target "${MOS_RUST_TARGET}"
        && ${CMAKE_COMMAND} -E copy ${OUTPUT_FILE} ${INITRD_DIR}/${INITRD_SUBDIR}
        VERBATIM
        WORKING_DIRECTORY ${PROJECT_DIR}
        DEPENDS ${PROJECT_DIR}/Cargo.toml
        BYPRODUCTS ${OUTPUT_FILE}
        COMMENT "Building ${NAME} (rust)"
    )

    foreach(DEPENDENCY ${ARGS_DEPENDS})
        add_dependencies(${NAME}_rust ${DEPENDENCY})
    endforeach(DEPENDENCY)

    add_dependencies(mos_initrd ${NAME}_rust)
endmacro()
