# SPDX-License-Identifier: GPL-3.0-or-later

make_directory(${CMAKE_BINARY_DIR}/boot.dir)

function(prepare_bootable_kernel_binary TARGET_NAME LINKER_SCRIPT)
    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "Target ${TARGET_NAME} does not exist")
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/boot.dir
        OUTPUT_NAME ${TARGET_NAME}
        EXCLUDE_FROM_ALL TRUE
        LINKER_LANGUAGE C
        LINK_DEPENDS "${LINKER_SCRIPT}"
        LINK_OPTIONS "-T${LINKER_SCRIPT};-nostdlib")

    # target_link_options(${TARGET_NAME} PRIVATE "-Wl,-Map=output.map")
    target_link_libraries(${TARGET_NAME} PRIVATE gcc mos::kernel)
endfunction()
