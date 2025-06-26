# SPDX-License-Identifier: GPL-3.0-or-later

set(KERNEL_MODULES_DIR "${CMAKE_BINARY_DIR}/modules")
make_directory("${KERNEL_MODULES_DIR}")

function(mos_add_kernel_module name )
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES)

    cmake_parse_arguments(MODULE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT MODULE_SOURCES)
        message(FATAL_ERROR "No sources provided for kernel module ${name}")
    endif()

    add_library(${name} STATIC ${MODULE_SOURCES})
    target_link_libraries(${name}
        PRIVATE
            mos::kernel_compile_options
            mos::include
            mos::private_include
            mos::kernel
    )

    target_compile_options(${name}
        PRIVATE
            -fvisibility=hidden)

    target_compile_definitions(${name}
        PRIVATE
            MOS_IS_KERNEL_MODULE
            MOS_MODULE_NAME="${name}"
    )

    set(OUTPUT_FILE_PATH "${KERNEL_MODULES_DIR}/${name}.o")

    add_custom_command(
        TARGET ${name}
        POST_BUILD
        COMMAND ${MOS_ARCH}-mos-ld "-r;-T;${CMAKE_SOURCE_DIR}/kernel/kmodule.ld;-o;${OUTPUT_FILE_PATH};$<JOIN:$<TARGET_OBJECTS:${name}>,;>"
        COMMENT "Creating relocatable object file for ${name}"
        VERBATIM
        COMMAND_EXPAND_LISTS
    )
    set_target_properties(${name} PROPERTIES OUTPUT_NAME ${name})
    set_target_properties(${name} PROPERTIES PREFIX "")
    set_target_properties(${name} PROPERTIES SUFFIX ".mod.a")
endfunction(mos_add_kernel_module)
