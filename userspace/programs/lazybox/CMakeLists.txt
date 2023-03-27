# SPDX-License-Identifier: GPL-3.0-or-later
# think of it like the BusyBox

macro(lazybox_program name description)
    add_executable(${name} ${name}.c)
    setup_userspace_program(${name} /programs "${description}")
endmacro()

lazybox_program(cat "Concatenate files and print on the standard output")
lazybox_program(false "Do nothing, unsuccessfully")
lazybox_program(ls "List directory contents")
lazybox_program(mkdir "Create directories")
lazybox_program(shutdown "Shutdown the system")
lazybox_program(stat "Display file or file system status")
lazybox_program(touch "Change file timestamps")
lazybox_program(tree "List contents of directories in a tree-like format")
lazybox_program(true "Do nothing, successfully")
