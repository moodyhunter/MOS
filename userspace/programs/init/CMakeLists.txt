# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT INIT_NO_BOOTSTRAPPER)
    add_subdirectory(bootstrapper)
else()
    message(STATUS "Skipping bootstrapper")
endif()

find_package(tomlplusplus REQUIRED)

add_executable(init
    main.cpp
    units/unit.cpp
    units/mount.cpp
    units/path.cpp
    units/service.cpp
    units/target.cpp
    units/symlink.cpp
    units/device.cpp
    units/template.cpp
    units/inherited.cpp
    common/ConfigurationManager.cpp
    parser.cpp
    ServiceManager.cpp
    rpc/rpc.cpp
    rpc/UnitStateReceiver.cpp
    utils/ExecUtils.cpp
)

target_link_libraries(init
    PRIVATE
        mos::argparse
        mos::include
        tomlplusplus::tomlplusplus
        mos::librpc-server
        mos::rpc-protocols
        stdc++exp
)

target_include_directories(init PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_to_initrd(TARGET init /programs)
