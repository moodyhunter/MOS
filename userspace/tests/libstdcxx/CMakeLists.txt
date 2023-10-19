# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(libstdc++-test
    main.cpp
)

add_to_initrd(TARGET libstdc++-test "/tests/")
