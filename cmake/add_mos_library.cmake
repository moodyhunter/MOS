# SPDX-License-Identifier: GPL-3.0-or-later

macro(add_mos_library)
    set(options USERSPACE_ONLY)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES RELATIVE_SOURCES PUBLIC_INCLUDE_DIRECTORIES PRIVATE_INCLUDE_DIRECTORIES USERSPACE_LINK_LIBRARIES)
    cmake_parse_arguments(ADD_MOS_LIBRARY "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ADD_MOS_LIBRARY_NAME)
        message(FATAL_ERROR "add_mos_library() requires a NAME")
    endif()

    if (ADD_MOS_LIBRARY_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "add_mos_library() was passed unknown arguments: ${ADD_MOS_LIBRARY_UNPARSED_ARGUMENTS}")
    endif()

    if (NOT ADD_MOS_LIBRARY_USERSPACE_ONLY)
        # Add the kernel source files
        add_kernel_source(
            SOURCES ${ADD_MOS_LIBRARY_SOURCES}
            RELATIVE_SOURCES ${ADD_MOS_LIBRARY_RELATIVE_SOURCES}
            INCLUDE_DIRECTORIES ${ADD_MOS_LIBRARY_PUBLIC_INCLUDE_DIRECTORIES}
        )
        set_source_files_properties(${ADD_MOS_LIBRARY_SOURCES} PROPERTIES INCLUDE_DIRECTORIES "${ADD_MOS_LIBRARY_PRIVATE_INCLUDE_DIRECTORIES}")
    endif()


    # Create a userspace library
    add_library(${ADD_MOS_LIBRARY_NAME} ${ADD_MOS_LIBRARY_SOURCES} ${ADD_MOS_LIBRARY_RELATIVE_SOURCES})
    target_include_directories(${ADD_MOS_LIBRARY_NAME} PUBLIC ${ADD_MOS_LIBRARY_PUBLIC_INCLUDE_DIRECTORIES})
    target_include_directories(${ADD_MOS_LIBRARY_NAME} PRIVATE ${ADD_MOS_LIBRARY_PRIVATE_INCLUDE_DIRECTORIES})
    target_link_libraries(${ADD_MOS_LIBRARY_NAME} PRIVATE ${ADD_MOS_LIBRARY_USERSPACE_LINK_LIBRARIES})
    target_link_libraries(${ADD_MOS_LIBRARY_NAME} PRIVATE gcc mos::include) # standard include directory

    # TODO: Remove this once we have a proper userspace libc
    target_compile_options(${ADD_MOS_LIBRARY_NAME} PUBLIC "-ffreestanding")
    target_link_options(${ADD_MOS_LIBRARY_NAME} PUBLIC "-nostdlib")

    add_library(mos::${ADD_MOS_LIBRARY_NAME} ALIAS ${ADD_MOS_LIBRARY_NAME})
endmacro()
