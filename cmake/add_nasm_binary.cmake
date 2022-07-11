# SPDX-License-Identifier: GPL-3.0-or-later

find_program(NASM NAMES ${CUSTOM_NASM_PATH} nasm REQUIRED)
message(STATUS "Found NASM at: ${NASM}")

if(NOT NASM)
message(FATAL_ERROR
    " \n"
    " NASM is not available.\n"
    " \n"
    " Please install NASM and try again.\n"
    " Don't forget to configure the path to NASM in 'config.cmake'. \n"
    " \n")
endif()

macro(add_nasm_binary TARGET)
    message(STATUS "Adding a new NASM binary target: ${TARGET}")
    if(TARGET ${TARGET})
        message(FATAL_ERROR
            " \n"
            " There has already been a target named:\n"
            "     '${TARGET}'\n"
            " \n"
            " Please suggest a NEW target name.\n"
            " \n")
    endif()

    set(single_val_arg SOURCE)
    cmake_parse_arguments(ASSEMBLY_NEW_TARGET "" "${single_val_arg}" "" ${ARGN})

    # If someone passed a relative path, make it absolute first.
    get_filename_component(ASSEMBLY_NEW_TARGET_SOURCE ${ASSEMBLY_NEW_TARGET_SOURCE} ABSOLUTE)

    # object_name: foo
    get_filename_component(object_name ${ASSEMBLY_NEW_TARGET_SOURCE} NAME_WLE)

    # relative_source_file: ./path/to/foo.asm
    file(RELATIVE_PATH relative_source_file ${CMAKE_SOURCE_DIR} ${ASSEMBLY_NEW_TARGET_SOURCE})

    # relative_source_dir: ./path/to/
    get_filename_component(relative_source_dir ${relative_source_file} DIRECTORY)

    message(STATUS "  ${TARGET}: ${relative_source_file} ==NASM=>> ${relative_source_dir}/${object_name}")

    add_custom_target(${TARGET} COMMENT "NASM assembly target" SOURCES ${ASSEMBLY_NEW_TARGET_SOURCE})
    add_custom_command(
        COMMAND
            ${NASM}
                ${ASSEMBLY_NEW_TARGET_SOURCE}
                -o ${CMAKE_CURRENT_BINARY_DIR}/${relative_source_dir}/${object_name}
                -I ${CMAKE_CURRENT_SOURCE_DIR}/${relative_source_dir}
        DEPENDS "${ASSEMBLY_NEW_TARGET_SOURCE}"
        TARGET ${TARGET}
        VERBATIM
        COMMENT "Assembling ${TARGET} with NASM (${NASM})..."
    )

    # small ninja hack
    set_target_properties(${TARGET} PROPERTIES EchoString "Assembling ${TARGET} with NASM (${NASM})...")

    # make directory: BUILDDIR/path/to/
    make_directory(${CMAKE_CURRENT_BINARY_DIR}/${relative_source_dir})
endmacro()
