# SPDX-License-Identifier: GPL-3.0-or-later

set(MOS_KERNEL_VERSION_STRING "${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}")

function(generate_kconfig TARGET)
    message(STATUS "Generating kconfig.c according to configuration...")

    execute_process(
        COMMAND git describe --long --tags --all --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE MOS_KERNEL_REVISION_STRING
        ERROR_VARIABLE _DROP_
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    set(MOS_KERNEL_REVISION_STRING "${MOS_KERNEL_REVISION_STRING}" PARENT_SCOPE)

    if(NOT MOS_KERNEL_BUILTIN_CMDLINE_STRING)
        set(MOS_KERNEL_BUILTIN_CMDLINE_STRING "")
    endif()

    make_directory(${CMAKE_BINARY_DIR}/include)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/kconfig.c.in ${CMAKE_BINARY_DIR}/kconfig.c)
    target_sources(${TARGET} PRIVATE ${CMAKE_BINARY_DIR}/kconfig.c)

    configure_file(${CMAKE_SOURCE_DIR}/cmake/kconfig.h.in ${CMAKE_BINARY_DIR}/include/mos/kconfig.h)
    target_sources(${TARGET} PRIVATE ${CMAKE_BINARY_DIR}/include/mos/kconfig.h)
endfunction()
