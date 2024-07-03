# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(bootstrapper)

add_executable(init main.cpp)
target_link_libraries(init
    PRIVATE
        mos::argparse
        mos::libconfig
        mos::include
)
add_to_initrd(TARGET init /programs)
