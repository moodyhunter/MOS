# SPDX-License-Identifier: GPL-3.0-or-later

make_directory(${CMAKE_BINARY_DIR}/boot.dir)

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

    add_executable(${TARGET_NAME} ${KERNEL_BINARY_LOADER_ASM})
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/boot.dir
        OUTPUT_NAME ${TARGET_NAME}
        EXCLUDE_FROM_ALL TRUE
        LINKER_LANGUAGE C
        LINK_DEPENDS "${KERNEL_BINARY_LINKER_SCRIPT}"
        LINK_OPTIONS "-T${KERNEL_BINARY_LINKER_SCRIPT};-nostdlib")

    # target_link_options(${TARGET_NAME} PRIVATE "-Wl,-Map=output.map")
    target_link_libraries(${TARGET_NAME} PRIVATE gcc mos::kernel)
endfunction()
