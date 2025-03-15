# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT INIT_NO_BOOTSTRAPPER)
    add_subdirectory(bootstrapper)
else()
    message(STATUS "Skipping bootstrapper")
endif()

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
    ServiceManager.cpp
    rpc/rpc.cpp
)

target_link_libraries(init
    PRIVATE
        mos::argparse
        mos::include
        tomlplusplus::tomlplusplus
        mos::librpc-server
        mos::rpc-protocols
)

target_include_directories(init PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_to_initrd(TARGET init /programs)
