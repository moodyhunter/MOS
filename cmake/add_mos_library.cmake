# SPDX-License-Identifier: GPL-3.0-or-later

macro(add_mos_library_do_setup LIBNAME DEFINES PRIVATE_INCLUDE PUBLIC_INCLUDE)
    target_compile_definitions(${LIBNAME} PRIVATE ${DEFINES})
    target_include_directories(${LIBNAME} PRIVATE ${PRIVATE_INCLUDE})

    # only define the public include directories as SYSTEM for users, not the library itself
    target_include_directories(${LIBNAME} PRIVATE ${PUBLIC_INCLUDE})
    target_include_directories(${LIBNAME} SYSTEM INTERFACE ${PUBLIC_INCLUDE})
endmacro()

function(add_mos_library)
    set(options USERSPACE_ONLY HOSTED_ONLY)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES PUBLIC_INCLUDE_DIRECTORIES PRIVATE_INCLUDE_DIRECTORIES LINK_LIBRARIES DEFINES PRIVATE_DEFINES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ARG_NAME)
        message(FATAL_ERROR "add_mos_library() requires a NAME")
    endif()

    if (ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "add_mos_library() was passed unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if (NOT ARG_USERSPACE_ONLY)
        # Create a kernel library
        set(KERNEL_LIB_NAME ${ARG_NAME}_kernel)
        add_library(${KERNEL_LIB_NAME} STATIC ${ARG_SOURCES})
        add_library(mos::${KERNEL_LIB_NAME} ALIAS ${KERNEL_LIB_NAME})
        add_mos_library_do_setup(${KERNEL_LIB_NAME} "__MOS_KERNEL__;__IN_MOS_LIBS__" "${ARG_PRIVATE_INCLUDE_DIRECTORIES}" "${ARG_PUBLIC_INCLUDE_DIRECTORIES}")
        target_link_libraries(${KERNEL_LIB_NAME} PRIVATE gcc mos::include mos::private_include)
        target_link_libraries(mos_kernel PUBLIC ${KERNEL_LIB_NAME})
        target_compile_definitions(${KERNEL_LIB_NAME} PUBLIC ${ARG_DEFINES})
        target_compile_definitions(${KERNEL_LIB_NAME} PRIVATE ${ARG_PRIVATE_DEFINES})

        if ("${ARG_NAME}" STREQUAL "stdlib")
            target_link_libraries(${KERNEL_LIB_NAME} PUBLIC mos::kernel_compile_options)
        else()
            target_link_libraries(${KERNEL_LIB_NAME} PUBLIC mos::stdlib_kernel)
        endif()

        foreach(lib ${ARG_LINK_LIBRARIES})
            target_link_libraries(${KERNEL_LIB_NAME} PUBLIC ${lib}_kernel)
        endforeach()
    endif()

    # Create a userspace library
    if (NOT ARG_HOSTED_ONLY)
        # userspace library with minimal libc
        add_library(${ARG_NAME} STATIC ${ARG_SOURCES})
        add_library(mos::${ARG_NAME} ALIAS ${ARG_NAME})
        target_compile_definitions(${ARG_NAME} PUBLIC ${ARG_DEFINES})
        target_compile_definitions(${ARG_NAME} PRIVATE ${ARG_PRIVATE_DEFINES})
        add_mos_library_do_setup(${ARG_NAME} "" "${ARG_PRIVATE_INCLUDE_DIRECTORIES}" "${ARG_PUBLIC_INCLUDE_DIRECTORIES}")
    endif()

    if ("${ARG_NAME}" STREQUAL "stdlib")
        # only need to add these to stdlib
        target_link_libraries(${ARG_NAME} PUBLIC gcc mos::include)
        target_compile_options(${ARG_NAME} PUBLIC "-ffreestanding")
        target_compile_definitions(${ARG_NAME} PUBLIC "__MLIBC_ABI_ONLY")
        target_compile_definitions(${ARG_NAME} PUBLIC "__MOS_MINIMAL_LIBC__")
        target_link_options(${ARG_NAME} PUBLIC "-nostdlib")
        if (ARG_LINK_LIBRARIES)
            message(FATAL_ERROR "stdlib must not link to other libraries")
        endif()
        return()
    endif()

    if (NOT ARG_HOSTED_ONLY)
        # library with full libc
        target_link_libraries(${ARG_NAME} PUBLIC mos::stdlib)
        target_link_libraries(${ARG_NAME} PUBLIC ${ARG_LINK_LIBRARIES})
    endif()

    # Create a hosted userspace library
    add_library(${ARG_NAME}_hosted SHARED ${ARG_SOURCES})
    add_library(mos::${ARG_NAME}_hosted ALIAS ${ARG_NAME}_hosted)

    add_mos_library_do_setup(${ARG_NAME}_hosted "" "${ARG_PRIVATE_INCLUDE_DIRECTORIES}" "${ARG_PUBLIC_INCLUDE_DIRECTORIES}")
    target_compile_definitions(${ARG_NAME}_hosted PUBLIC ${ARG_DEFINES})
    target_compile_definitions(${ARG_NAME}_hosted PRIVATE ${ARG_PRIVATE_DEFINES})

    target_link_libraries(${ARG_NAME}_hosted PUBLIC mos::include)
    foreach(lib ${ARG_LINK_LIBRARIES})
        target_link_libraries(${ARG_NAME}_hosted PUBLIC ${lib}_hosted)
    endforeach()
endfunction()
