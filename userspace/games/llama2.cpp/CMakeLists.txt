# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(llama run.cpp)
add_to_initrd(TARGET llama /games)
