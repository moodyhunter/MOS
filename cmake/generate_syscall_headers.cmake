# SPDX-License-Identifier: GPL-3.0-or-later

find_program(PYTHON "python3" NAMES "python3 python" REQUIRED)

function(generate_syscall_headers  SYSCALL_JSON)
    make_directory("${CMAKE_BINARY_DIR}/include/mos/syscall/")

    add_custom_target(mos_syscall_decl SOURCES ${SYSCALL_JSON})

    foreach(TYPE decl dispatcher number usermode)
        set(OUTPUT_FILE ${CMAKE_BINARY_DIR}/include/mos/syscall/${TYPE}.h)
        add_custom_command(
            TARGET mos_syscall_decl
            MAIN_DEPENDENCY ${SYSCALL_JSON}
            BYPRODUCTS ${OUTPUT_FILE}
            DEPENDS ${CMAKE_SOURCE_DIR}/scripts/gen_syscall.py
            COMMAND ${PYTHON}
            ${CMAKE_SOURCE_DIR}/scripts/gen_syscall.py
            gen-${TYPE}
            ${SYSCALL_JSON}
            ${OUTPUT_FILE}
            VERBATIM
        )
    endforeach()
endfunction()
