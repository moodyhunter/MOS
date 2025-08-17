# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(display-manager
    main.cpp
    input/input.cpp
    windows/window-manager.cpp
    windows/window.cpp
    render/renderer.cpp
    utils/common.cpp
)
target_link_libraries(display-manager
  PRIVATE
    librpc::client
    librpc::server
    libsm::sm
    mos::rpc-protocols
)
add_to_initrd(TARGET display-manager /services)
add_to_initrd(FILE display-manager.toml /config/bits)

# inputd kernel module
mos_add_kernel_module(inputd-kmod NAME inputd SOURCES input/inputd-kmod.cpp)
target_include_directories(inputd-kmod PRIVATE include)
add_to_initrd(MODULE inputd-kmod /modules)
