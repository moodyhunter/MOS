# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(device-manager main.c)
setup_userspace_program(device-manager /programs "The device manager daemon")
target_include_directories(device-manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)
target_link_libraries(device-manager PRIVATE librpc::server)

add_subdirectory(lib)
