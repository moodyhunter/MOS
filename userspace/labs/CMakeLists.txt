# SPDX-License-Identifier: GPL-3.0-or-later

macro(add_lab name)
    add_executable(${name} ${name}.c)
    setup_userspace_program(${name} /tests "Test program for ${name}")
endmacro()

add_lab(lab2.1)

add_lab(lab3.1)
target_link_libraries(lab3.1 PRIVATE x86-console::client)
add_lab(lab3.2)
target_link_libraries(lab3.2 PRIVATE x86-console::client mos::private_include)
add_lab(lab3.3)
target_link_libraries(lab3.3 PRIVATE mos::private_include)

add_lab(lab4.1)
add_lab(lab4.2)
add_lab(lab4.3)
