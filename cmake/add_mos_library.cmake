# SPDX-License-Identifier: GPL-3.0-or-later
include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/MOSSDKConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/MOSSDKConfig.cmake"
  INSTALL_DESTINATION cmake/MOSSDK
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/MOSSDKConfigVersion.cmake"
  VERSION "1.0"
  COMPATIBILITY AnyNewerVersion
)


macro(add_mos_library_do_setup LIBNAME DEFINES PRIVATE_INCLUDE PUBLIC_INCLUDE)
    target_compile_definitions(${LIBNAME} PRIVATE ${DEFINES})
    target_include_directories(${LIBNAME} PRIVATE ${PRIVATE_INCLUDE})

    # only define the public include directories as SYSTEM for users, not the library itself
    target_include_directories(${LIBNAME} PRIVATE ${PUBLIC_INCLUDE})
    target_include_directories(${LIBNAME} SYSTEM INTERFACE ${PUBLIC_INCLUDE})
endmacro()

function(add_mos_library)
    set(options USERSPACE_ONLY KERNEL_ONLY)
    set(oneValueArgs NAME PUBLIC_HEADER_SET_BASE)
    set(multiValueArgs SOURCES PUBLIC_INCLUDE_DIRECTORIES PRIVATE_INCLUDE_DIRECTORIES LINK_LIBRARIES DEFINES PRIVATE_DEFINES PUBLIC_HEADER_SET PUBLIC_HEADERS)
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

    if ("${ARG_NAME}" STREQUAL "stdlib")
        add_library(stdlib_minimal STATIC ${ARG_SOURCES})
        add_library(mos::stdlib_minimal ALIAS stdlib_minimal)
        target_compile_definitions(stdlib_minimal PUBLIC ${ARG_DEFINES})
        target_compile_definitions(stdlib_minimal PRIVATE ${ARG_PRIVATE_DEFINES})
        add_mos_library_do_setup(stdlib_minimal "" "${ARG_PRIVATE_INCLUDE_DIRECTORIES}" "${ARG_PUBLIC_INCLUDE_DIRECTORIES}")

        target_link_libraries(stdlib_minimal PUBLIC gcc mos::include)
        target_compile_options(stdlib_minimal PUBLIC "-ffreestanding")
        target_compile_definitions(stdlib_minimal PUBLIC "__MLIBC_ABI_ONLY")
        target_compile_definitions(stdlib_minimal PUBLIC "__MOS_MINIMAL_LIBC__")
        target_link_options(stdlib_minimal PUBLIC "-nostdlib")
        if (ARG_LINK_LIBRARIES)
            message(FATAL_ERROR "stdlib must not link to other libraries")
        endif()
        return()
    endif()

    if (ARG_KERNEL_ONLY)
        return()
    endif()

    # Create a hosted shared library
    add_library(${ARG_NAME} SHARED ${ARG_SOURCES})
    add_library(mos::${ARG_NAME} ALIAS ${ARG_NAME})
    target_sources(${ARG_NAME} PUBLIC FILE_SET HEADERS BASE_DIRS ${ARG_PUBLIC_HEADER_SET_BASE} FILES ${ARG_PUBLIC_HEADER_SET})
    add_mos_library_do_setup(${ARG_NAME} "" "${ARG_PRIVATE_INCLUDE_DIRECTORIES}" "${ARG_PUBLIC_INCLUDE_DIRECTORIES}")
    target_compile_definitions(${ARG_NAME} PUBLIC ${ARG_DEFINES})
    target_compile_definitions(${ARG_NAME} PRIVATE ${ARG_PRIVATE_DEFINES})

    target_link_libraries(${ARG_NAME} PUBLIC mos::include)
    foreach(lib ${ARG_LINK_LIBRARIES})
        target_link_libraries(${ARG_NAME} PUBLIC ${lib})
    endforeach()

    install(TARGETS ${ARG_NAME}
        EXPORT MOSSDK
        COMPONENT sdk
        EXCLUDE_FROM_ALL
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        INCLUDES DESTINATION include
        FILE_SET HEADERS DESTINATION include/${ARG_NAME}
    )

    install(FILES ${ARG_PUBLIC_HEADERS}
        DESTINATION include/${ARG_NAME}
        COMPONENT sdk
        EXCLUDE_FROM_ALL
    )

    # create a hosted static library
    add_library(${ARG_NAME}_static STATIC ${ARG_SOURCES})
    add_library(mos::${ARG_NAME}_static ALIAS ${ARG_NAME}_static)

    add_mos_library_do_setup(${ARG_NAME}_static "" "${ARG_PRIVATE_INCLUDE_DIRECTORIES}" "${ARG_PUBLIC_INCLUDE_DIRECTORIES}")
    target_compile_definitions(${ARG_NAME}_static PUBLIC ${ARG_DEFINES})
    target_compile_definitions(${ARG_NAME}_static PRIVATE ${ARG_PRIVATE_DEFINES})

    target_link_libraries(${ARG_NAME}_static PUBLIC mos::include)
    foreach(lib ${ARG_LINK_LIBRARIES})
        target_link_libraries(${ARG_NAME}_static PUBLIC ${lib}_static)
    endforeach()
endfunction()

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/MOSSDKConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/MOSSDKConfigVersion.cmake"
    DESTINATION cmake/MOSSDK
    COMPONENT sdk
    EXCLUDE_FROM_ALL
)

install(EXPORT MOSSDK
    COMPONENT sdk
    EXCLUDE_FROM_ALL
    NAMESPACE mos::
    DESTINATION cmake/MOSSDK
    FILE MOSSDKTargets.cmake
)
