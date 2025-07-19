# SPDX-License-Identifier: GPL-3.0-or-later

set(LIB_ONLY ON)

add_definitions(-DCONFIG_USE_DEFAULT_CFG=1)
add_subdirectory(lwext4)

add_executable(ext4-daemon
    main.cpp
    ext4fs.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_link_libraries(ext4-daemon PRIVATE lwext4 blockdev-manager-lib)
target_include_directories(ext4-daemon PRIVATE lwext4/include)
target_compile_options(ext4-daemon PRIVATE -Wno-pedantic) # lwext4 is not pedantic clean

target_link_libraries(ext4-daemon PRIVATE
    mos::include
    mos::nanopb
    mos::librpc-server
    mos::librpc-client
    mos::rpc-protocols
    libsm::sm
)

add_to_initrd(TARGET ext4-daemon /services)
add_to_initrd(FILE ext4-daemon.toml /config/bits)
