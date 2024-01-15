# SPDX-License-Identifier: GPL-3.0-or-later

set(MOS_SYSCALL_DIR ${CMAKE_BINARY_DIR}/include/mos/syscall)
set(MOS_SYSCALL_FILES
    ${MOS_SYSCALL_DIR}/decl.h
    ${MOS_SYSCALL_DIR}/dispatcher.h
    ${MOS_SYSCALL_DIR}/number.h
    ${MOS_SYSCALL_DIR}/usermode.h
    ${MOS_SYSCALL_DIR}/table.h
)

find_program(PYTHON "python3" NAMES "python3 python" REQUIRED)
make_directory("${MOS_SYSCALL_DIR}")

set(MOS_SYSCALL_JSON ${CMAKE_SOURCE_DIR}/kernel/ksyscalls.json)
add_custom_command(
    OUTPUT ${MOS_SYSCALL_FILES}
    MAIN_DEPENDENCY ${MOS_SYSCALL_JSON}
    DEPENDS ${CMAKE_SOURCE_DIR}/scripts/gen_syscall.py
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/gen_syscall.py ${MOS_SYSCALL_JSON} ${MOS_SYSCALL_DIR}
    VERBATIM
)

add_custom_target(mos_syscall_decl DEPENDS ${MOS_SYSCALL_FILES})
add_summary_item(UTILITY mos_syscall_decl "" "Generate syscall headers")
