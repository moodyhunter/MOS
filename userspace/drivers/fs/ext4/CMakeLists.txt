# SPDX-License-Identifier: GPL-3.0-or-later

set(LIB_ONLY ON)

add_definitions(-DCONFIG_USE_DEFAULT_CFG=1)
add_subdirectory(lwext4)

add_executable(ext4fs
    main.cpp
    ext4fs.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_link_libraries(ext4fs PRIVATE lwext4 blockdev-manager-lib)
target_include_directories(ext4fs PRIVATE lwext4/include)
target_compile_options(ext4fs PRIVATE -Wno-pedantic) # lwext4 is not pedantic clean

target_link_libraries(ext4fs PRIVATE
    mos::include
    mos::nanopb
    mos::librpc-server
    mos::librpc-client
    mos::rpc-protocols
    libsm::sm
)

add_to_initrd(TARGET ext4fs /drivers/)
