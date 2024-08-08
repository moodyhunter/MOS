# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(mossh
    main.cpp
    builtin.cpp
    utils.cpp
)

target_include_directories(mossh PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mossh PRIVATE mos::argparse mos::libreadline)
add_to_initrd(TARGET mossh /programs)
