# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT MOS_ARCH STREQUAL "x86_64")
    message(WARNING "ACPI daemon is only supported on x86_64")
    return()
endif()

include(uACPI/uacpi.cmake)

add_executable(acpi-daemon
    main.cpp
    uacpi_host.cpp
    ${UACPI_SOURCES}
)

target_include_directories(acpi-daemon
    PRIVATE
    ${UACPI_INCLUDES}
)

set_source_files_properties(${UACPI_SOURCES} PROPERTIES COMPILE_FLAGS "-Wno-format -Wno-pedantic")

target_link_libraries(acpi-daemon PRIVATE mos::private_include)
add_to_initrd(TARGET acpi-daemon /drivers)
