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

    set(mos_kernel_objects_input "$<TARGET_OBJECTS:${KERNEL_BINARY_LOADER_ASSEMBLY_TARGET}>;$<TARGET_OBJECTS:mos_kernel_object>")
    add_custom_command(
        OUTPUT ${MOS_KERNEL_BINARY}
        COMMENT "Linking MOS kernel object..."
        DEPENDS ${mos_kernel_objects_input} ${KERNEL_BINARY_LINKER_SCRIPT}
        VERBATIM
        COMMAND
            bash -c "export OBJS='${mos_kernel_objects_input}' && ld -melf_i386 -o ${MOS_KERNEL_BINARY} -T${KERNEL_BINARY_LINKER_SCRIPT} \${OBJS//;/ }"
    )
    add_custom_target(mos_kernel ALL DEPENDS mos_kernel_object ${KERNEL_BINARY_LOADER_ASSEMBLY_TARGET} ${MOS_KERNEL_BINARY})
endfunction()
