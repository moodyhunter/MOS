# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT MOS_ARCH STREQUAL "x86")
    return()
endif()

add_executable(x86_console_driver main.c text_mode_console.c)
target_link_libraries(x86_console_driver PRIVATE mos::private_include mos::librpc-server)
setup_userspace_program(x86_console_driver /drivers "x86 console userspace driver")

add_library(x86_console-client STATIC console-client.c)
target_link_libraries(x86_console-client PRIVATE mos::libuserspace mos::private_include mos::librpc-client)
target_include_directories(x86_console-client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
