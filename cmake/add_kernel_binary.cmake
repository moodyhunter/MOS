# SPDX-License-Identifier: GPL-3.0-or-later

function(add_kernel_binary)
    set(options)
    set(one_value_arg "LOADER_ASSEMBLY_TARGET" "LINKER_SCRIPT")
    set(multi_value_args)
    cmake_parse_arguments(KERNEL_BINARY "${options}" "${one_value_arg}" "${multi_value_args}" "${ARGN}")

    if(NOT KERNEL_BINARY_LOADER_ASSEMBLY_TARGET)
        message(FATAL "LOADER_ASSEMBLY_TARGET is not set")
    endif()

    if(NOT KERNEL_BINARY_LINKER_SCRIPT)
        message(FATAL_ERROR "No LINKER_SCRIPT specified")
    endif()

    add_executable(mos_kernel.bin $<TARGET_OBJECTS:${KERNEL_BINARY_LOADER_ASSEMBLY_TARGET}> $<TARGET_OBJECTS:mos_kernel_object>)
    set_property(TARGET mos_kernel.bin PROPERTY LINK_OPTIONS -melf_i386 -T${KERNEL_BINARY_LINKER_SCRIPT})
    add_dependencies(mos_kernel.bin mos_kernel_object ${KERNEL_BINARY_LOADER_ASSEMBLY_TARGET})
endfunction()
