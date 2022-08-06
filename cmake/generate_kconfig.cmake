# SPDX-License-Identifier: GPL-3.0-or-later

set(MOS_KERNEL_VERSION_STRING "${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}")

set(MOS_KCONFIG_DEFINES)

macro(mos_add_kconfig_define CONFIG)
    list(APPEND MOS_KCONFIG_DEFINES "${CONFIG}")
    set(_MOS_KCONFIG_DEFINES_${CONFIG}_file "${CMAKE_CURRENT_LIST_FILE}")
endmacro()

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

    make_directory(${CMAKE_BINARY_DIR}/include)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/kconfig.c.in ${CMAKE_BINARY_DIR}/kconfig.c)
    target_sources(${TARGET} PRIVATE ${CMAKE_BINARY_DIR}/kconfig.c)

    set(KCONFIG_H "${CMAKE_BINARY_DIR}/include/mos/kconfig.h")

    configure_file(${CMAKE_SOURCE_DIR}/cmake/kconfig.h.in "${KCONFIG_H}")
    target_sources(${TARGET} PRIVATE "${KCONFIG_H}")

    set(_prev_file "")
    foreach(CONFIG ${MOS_KCONFIG_DEFINES})
        message(STATUS "  nanana ${CONFIG}")
        set(_file "${_MOS_KCONFIG_DEFINES_${CONFIG}_file}")
        set(_val "${${CONFIG}}")

        if(_val STREQUAL "ON" OR _val STREQUAL "YES" OR _val STREQUAL "TRUE" OR _val STREQUAL "1")
            set(_val 1)
        elseif(_val STREQUAL "OFF" OR _val STREQUAL "NO" OR _val STREQUAL "FALSE" OR _val STREQUAL "0")
            set(_val 0)
        endif()

        if(NOT _file STREQUAL _prev_file)
            file(APPEND ${KCONFIG_H} "\n// defined in ${_file}\n")
            set(_prev_file "${_file}")
        endif()
        file(APPEND ${KCONFIG_H} "#define ${CONFIG} ${_val}\n")
    endforeach()
endfunction()
