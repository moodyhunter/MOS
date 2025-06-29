# SPDX-License-Identifier: GPL-3.0-or-later

set(KERNEL_MODULES_DIR "${CMAKE_BINARY_DIR}/modules")
make_directory("${KERNEL_MODULES_DIR}")

function(mos_add_kernel_module target)
    set(options)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES)

    cmake_parse_arguments(MODULE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT MODULE_SOURCES)
        message(FATAL_ERROR "No sources provided for kernel module ${target}")
    endif()

    if(NOT MODULE_NAME)
        message(FATAL_ERROR "No name provided for kernel module")
    endif()

    add_library(${target} STATIC ${MODULE_SOURCES})
    target_link_libraries(${target}
        PRIVATE
            mos::kernel_compile_options
            mos::include
            mos::private_include
            mos::kernel
    )

    target_compile_options(${target}
        PRIVATE
            -fvisibility=hidden)

    target_compile_definitions(${target}
        PRIVATE
            MOS_IS_KERNEL_MODULE
            MOS_MODULE_NAME="${MODULE_NAME}"
    )

    set(OUTPUT_FILE_PATH "${KERNEL_MODULES_DIR}/${MODULE_NAME}.ko")

    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${MOS_ARCH}-mos-ld "-r;-T;${CMAKE_SOURCE_DIR}/kernel/kmodule.ld;-o;${OUTPUT_FILE_PATH};$<JOIN:$<TARGET_OBJECTS:${target}>,;>"
        COMMENT "Creating relocatable object file for kernel module ${MODULE_NAME}"
        VERBATIM
        COMMAND_EXPAND_LISTS
    )
    set_target_properties(${target} PROPERTIES OUTPUT_NAME ${MODULE_NAME})
    set_target_properties(${target} PROPERTIES OUTPUT_KERNEL_MODULE_PATH ${OUTPUT_FILE_PATH})
    set_target_properties(${target} PROPERTIES PREFIX "")
    set_target_properties(${target} PROPERTIES SUFFIX ".mod.a")
endfunction(mos_add_kernel_module)
