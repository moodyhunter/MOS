# SPDX-License-Identifier: GPL-3.0-or-later

find_program(PYTHON "python3" NAMES "python3.10 python3.9 python3.8 python3.7 python3.6 python python3" REQUIRED)

function(generate_syscall_headers TARGET SYSCALL_JSON)
    make_directory("${CMAKE_BINARY_DIR}/include/mos/syscall/")

    foreach(TYPE decl dispatcher number usermode)
        set(OUTPUT_FILE ${CMAKE_BINARY_DIR}/include/mos/syscall/${TYPE}.h)
        add_custom_command(OUTPUT ${OUTPUT_FILE}
            MAIN_DEPENDENCY ${SYSCALL_JSON}
            DEPENDS ${CMAKE_SOURCE_DIR}/scripts/gen_syscall.py
            COMMAND ${PYTHON}
            ${CMAKE_SOURCE_DIR}/scripts/gen_syscall.py
            gen-${TYPE}
            ${SYSCALL_JSON}
            ${OUTPUT_FILE}
            VERBATIM
        )
        target_sources(${TARGET} PUBLIC ${OUTPUT_FILE})
    endforeach()
endfunction()
