# SPDX-License-Identifier: GPL-3.0-or-later
# think of it like the BusyBox

macro(lazybox_program name description)
    add_executable(${name} ${name}.c mosapi.c)
    target_link_libraries(${name} PRIVATE mos::stdlib_minimal)
    add_to_initrd(TARGET ${name} /programs)
endmacro()

lazybox_program(cat "Concatenate files and print on the standard output")
lazybox_program(false "Do nothing, unsuccessfully")
lazybox_program(kill "Send signal to process")
lazybox_program(ls "List directory contents")
lazybox_program(mkdir "Create directories")
lazybox_program(shutdown "Shutdown the system")
lazybox_program(stat "Display file or file system status")
lazybox_program(touch "Change file timestamps")
lazybox_program(tree "List contents of directories in a tree-like format")
lazybox_program(true "Do nothing, successfully")
lazybox_program(clear "Clear the terminal screen")
lazybox_program(rm "Remove files or directories")
lazybox_program(write "A simple text writer")
lazybox_program(symlink "Create a symbolic link")
lazybox_program(env "Print environment variables")
lazybox_program(echo "Display a line of text")
lazybox_program(pwd "Print name of current/working directory")
