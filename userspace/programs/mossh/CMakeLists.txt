# SPDX-License-Identifier: GPL-3.0-or-later

find_package(nlohmann_json REQUIRED)

add_executable(mossh
    main.cpp
    builtin.cpp
    utils.cpp
    parser.cpp
    jsonrpc.cpp
    LaunchContext.cpp
)

target_include_directories(mossh PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mossh PRIVATE mos::argparse mos::libreadline nlohmann_json::nlohmann_json)
add_to_initrd(TARGET mossh /programs)
