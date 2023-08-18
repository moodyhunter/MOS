# SPDX-License-Identifier: GPL-3.0-or-later
set(MOS_SUMMARY_NAME_LENGTH 35)
set(MOS_SUMMARY_DESCRIPTION_LENGTH 40)

set(_MOS_SUMMARY_SECTION_ORDER "" CACHE INTERNAL "" FORCE)

macro(add_summary_section SECTION_ID NAME)
    list(APPEND _MOS_SUMMARY_SECTION_ORDER ${SECTION_ID})

    set(_MOS_SUMMARY_SECTION_ORDER "${_MOS_SUMMARY_SECTION_ORDER}" CACHE INTERNAL "" FORCE)
    set(_MOS_SUMMARY_SECTION_${SECTION_ID} "${NAME}" CACHE INTERNAL "" FORCE)
    set(_MOS_SUMMARY_SECTION_CONTENT_${SECTION_ID} "" CACHE INTERNAL "" FORCE)
endmacro()

macro(add_summary_item SECTION_ID VARIABLE VALUE DESCRIPTION)
    if(NOT DEFINED _MOS_SUMMARY_SECTION_${SECTION_ID})
        message(FATAL_ERROR "Unknown summary section '${SECTION_ID}'")
    endif()

    string(LENGTH ${VARIABLE} NAME_LEN)
    math(EXPR padding "${MOS_SUMMARY_NAME_LENGTH} - ${NAME_LEN} - 2")

    if(padding LESS 0)
        set(padding 0)
    endif()

    string(REPEAT "." ${padding} PADDING_STRING_NAME)

    string(LENGTH "${DESCRIPTION}" DESC_LEN)
    math(EXPR padding "${MOS_SUMMARY_DESCRIPTION_LENGTH} - ${DESC_LEN} - 2")

    if(padding LESS 0)
        set(padding 0)
    endif()

    string(REPEAT "." ${padding} PADDING_STRING_DESC)

    if (NOT "${VALUE}" STREQUAL "")
        set(PRETTY_VALUE " = ${VALUE}")
    else()
        set(PRETTY_VALUE "")
    endif()
    list(APPEND _MOS_SUMMARY_SECTION_CONTENT_${SECTION_ID} "${DESCRIPTION} ${PADDING_STRING_DESC}${PADDING_STRING_NAME} ${VARIABLE}${PRETTY_VALUE}")
    set(_MOS_SUMMARY_SECTION_CONTENT_${SECTION_ID} "${_MOS_SUMMARY_SECTION_CONTENT_${SECTION_ID}}" CACHE INTERNAL "" FORCE)
endmacro()

function(print_summary)
    message("Configuration Summary:")

    foreach(section ${_MOS_SUMMARY_SECTION_ORDER})
        message("  ${_MOS_SUMMARY_SECTION_${section}}")

        foreach(item ${_MOS_SUMMARY_SECTION_${section}})
            foreach(item ${_MOS_SUMMARY_SECTION_CONTENT_${section}})
                message("    ${item}")
            endforeach()
        endforeach()

        message("")
    endforeach()

    mos_save_summary()
endfunction()

function(mos_save_summary)
    set(SUMMARY_FILE "${CMAKE_BINARY_DIR}/summary.txt")
    file(WRITE ${SUMMARY_FILE} "Configuration Summary:\n")

    foreach(section ${_MOS_SUMMARY_SECTION_ORDER})
        file(APPEND ${SUMMARY_FILE} "  ${_MOS_SUMMARY_SECTION_${section}}\n")

        foreach(item ${_MOS_SUMMARY_SECTION_CONTENT_${section}})
            file(APPEND ${SUMMARY_FILE} "    ${item}\n")
        endforeach()

        file(APPEND ${SUMMARY_FILE} "\n")
    endforeach()
endfunction()
