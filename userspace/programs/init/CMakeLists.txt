# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(bootstrapper)

find_package(tomlplusplus REQUIRED)

add_executable(init
    main.cpp
    unit/unit.cpp
    unit/mount.cpp
    unit/path.cpp
    unit/service.cpp
    unit/target.cpp
    unit/symlink.cpp
    parser.cpp
)
target_link_libraries(init
    PRIVATE
        mos::argparse
        mos::include
        tomlplusplus::tomlplusplus
)
add_to_initrd(TARGET init /programs)
