# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT MOS_ARCH STREQUAL "x86")
    return()
endif()

add_executable(x86_console_driver main.c text_mode_console.c)
target_link_libraries(x86_console_driver PRIVATE mos::stdlib mos::private_include librpc::server)
target_include_directories(x86_console_driver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)
setup_userspace_program(x86_console_driver /drivers "x86 console userspace driver")

add_subdirectory(lib)
