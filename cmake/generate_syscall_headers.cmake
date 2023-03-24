# SPDX-License-Identifier: GPL-3.0-or-later
set(MOS_SYSCALL_DIR ${CMAKE_BINARY_DIR}/include/mos/syscall)
set(MOS_SYSCALL_FILES
    ${MOS_SYSCALL_DIR}/decl.h
    ${MOS_SYSCALL_DIR}/dispatcher.h
    ${MOS_SYSCALL_DIR}/number.h
    ${MOS_SYSCALL_DIR}/usermode.h
)


function(generate_syscall_headers SYSCALL_JSON)
    find_program(PYTHON "python3" NAMES "python3 python" REQUIRED)
    make_directory("${MOS_SYSCALL_DIR}")

    add_custom_command(
        OUTPUT ${MOS_SYSCALL_FILES}
        MAIN_DEPENDENCY ${SYSCALL_JSON}
        BYPRODUCTS
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/gen_syscall.py
        COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/gen_syscall.py ${SYSCALL_JSON} ${MOS_SYSCALL_DIR}
        VERBATIM
    )

    add_custom_target(mos_syscall_decl DEPENDS ${MOS_SYSCALL_FILES})
    add_summary_item(UTILITY mos_syscall_decl "" "Generate syscall headers")
endfunction()
