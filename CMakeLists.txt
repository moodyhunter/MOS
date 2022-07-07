project(MOS LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(mos_c ${CMAKE_CURRENT_SOURCE_DIR}/main.c)

target_link_options(mos_c PRIVATE -nostdlib)

message("MOS is now configured.")
