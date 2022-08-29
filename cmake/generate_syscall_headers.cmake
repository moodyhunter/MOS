# SPDX-License-Identifier: GPL-3.0-or-later

find_program(PYTHON "python3" NAMES "python3.10 python3.9 python3.8 python3.7 python3.6 python3.5 python3.4 python3.3 python3.2 python3.1 python3.0 python python3" REQUIRED)

function(generate_syscall_headers TARGET SYSCALL_JSON PREFIX PREFIX_DIR)
    make_directory("${CMAKE_BINARY_DIR}/include/${PREFIX_DIR}/${PREFIX}/")
    foreach(TYPE decl dispatcher number)
        set(OUTPUT_FILE ${CMAKE_BINARY_DIR}/include/${PREFIX_DIR}/${PREFIX}/${PREFIX}_${TYPE}.h)
        add_custom_command(OUTPUT ${OUTPUT_FILE}
            MAIN_DEPENDENCY ${SYSCALL_JSON}
            DEPENDS ${CMAKE_SOURCE_DIR}/scripts/gen_syscall.py
            COMMAND ${PYTHON}
                ${CMAKE_SOURCE_DIR}/scripts/gen_syscall.py
                ${PREFIX}
                gen-${TYPE}
                ${SYSCALL_JSON}
                ${OUTPUT_FILE}
            VERBATIM
        )
        target_sources(${TARGET} PUBLIC ${OUTPUT_FILE})
    endforeach()
endfunction()
