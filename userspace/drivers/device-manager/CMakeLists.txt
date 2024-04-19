# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(device-manager main.c dm_server.c drivers.c)
target_include_directories(device-manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)
target_link_libraries(device-manager PRIVATE librpc::server mos::libconfig mos::argparse)
add_to_initrd(TARGET device-manager /drivers)
add_subdirectory(lib)
