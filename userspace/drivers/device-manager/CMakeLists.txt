# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(device-manager main.cpp dm_server.cpp drivers.cpp)

target_include_directories(device-manager
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
)

target_link_libraries(device-manager
    PRIVATE
    librpc::server
    mos::config
    mos::argparse
    libsm::sm
)

add_to_initrd(TARGET device-manager /drivers)

add_subdirectory(lib)
