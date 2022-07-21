# SPDX-License-Identifier: GPL-3.0-or-later

make_directory(${CMAKE_BINARY_DIR}/boot)

function(prepare_bootable_kernel_binary TARGET_NAME)
    set(options)
    set(one_value_arg "LOADER_ASM" "LINKER_SCRIPT")
    set(multi_value_args)
    cmake_parse_arguments(KERNEL_BINARY "${options}" "${one_value_arg}" "${multi_value_args}" "${ARGN}")

    if(NOT KERNEL_BINARY_LOADER_ASM)
        message(FATAL_ERROR "LOADER_ASM is not set")
    endif()

    if(NOT KERNEL_BINARY_LINKER_SCRIPT)
        message(FATAL_ERROR "No LINKER_SCRIPT specified")
    endif()

    if(KERNEL_BINARY_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown argument when calling 'prepare_bootable_kernel_binary': ${KERNEL_BINARY_UNPARSED_ARGUMENTS}")
    endif()

    set(LOADER_TARGET mos_kernel_${TARGET_NAME}_loader)

    add_nasm_binary(${LOADER_TARGET} SOURCE ${KERNEL_BINARY_LOADER_ASM} ELF_OBJECT)

    add_executable(${TARGET_NAME})
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/boot
        OUTPUT_NAME ${TARGET_NAME}
        EXCLUDE_FROM_ALL TRUE
        LINKER_LANGUAGE C
        LINK_DEPENDS "${KERNEL_BINARY_LINKER_SCRIPT}"
        # Do not link with 'mos::elf_kernel' in CMake because it is not a library, nor an exported executable.
        LINK_OPTIONS "-T${KERNEL_BINARY_LINKER_SCRIPT}")
    target_link_libraries(${TARGET_NAME} mos::elf_kernel ${LOADER_TARGET}::object)
    add_dependencies(${TARGET_NAME} ${LOADER_TARGET} mos::elf_kernel)
endfunction()
