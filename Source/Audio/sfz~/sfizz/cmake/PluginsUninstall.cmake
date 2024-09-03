# SPDX-License-Identifier: BSD-2-Clause

# This code is part of the sfizz library and is licensed under a BSD 2-clause
# license. You should have receive a LICENSE.md file along with the code.
# If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MakeUninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/MakeUninstall.cmake"
        IMMEDIATE @ONLY)

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/MakeUninstall.cmake)

    if(PLUGIN_LV2 AND LV2_PLUGIN_INSTALL_DIR)
        add_custom_command(TARGET uninstall
            COMMAND rm -rv "${LV2_PLUGIN_INSTALL_DIR}/${PROJECT_NAME}.lv2")
    endif()

    if(PLUGIN_VST3 AND VST3_PLUGIN_INSTALL_DIR)
        add_custom_command(TARGET uninstall
            COMMAND rm -rv "${VST3_PLUGIN_INSTALL_DIR}/${PROJECT_NAME}.vst3")
    endif()
endif()
