# SPDX-License-Identifier: BSD-2-Clause

# This code is part of the sfizz library and is licensed under a BSD 2-clause
# license. You should have receive a LICENSE.md file along with the code.
# If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

# This option is for MIDI CC support in absence of host midi:binding support
option(PLUGIN_LV2_PSA "Enable plugin-side MIDI automations" ON)

set(LV2_PLUGIN_NAME       "${PROJECT_NAME}")
set(LV2_PLUGIN_COMMENT    "${PROJECT_DESCRIPTION}")
set(LV2_PLUGIN_URI        "${PROJECT_HOMEPAGE_URL}")
set(LV2_PLUGIN_REPOSITORY "${PROJECT_REPOSITORY}")
set(LV2_PLUGIN_AUTHOR     "${PROJECT_AUTHOR}")
set(LV2_PLUGIN_EMAIL      "${PROJECT_EMAIL}")

if(NOT LV2_PLUGIN_SPDX_LICENSE_ID)
    set(LV2_PLUGIN_SPDX_LICENSE_ID "ISC") # default license
endif()

if(PLUGIN_LV2_UI)
    set(LV2_PLUGIN_IF_ENABLE_UI "")
else()
    set(LV2_PLUGIN_IF_ENABLE_UI "#")
endif()

if(WIN32)
    set(LV2_UI_TYPE "WindowsUI")
elseif(APPLE)
    set(LV2_UI_TYPE "CocoaUI")
elseif(HAIKU)
    set(LV2_UI_TYPE "BeUI")
else()
    set(LV2_UI_TYPE "X11UI")
endif()

if(APPLE)
    set(LV2_PLUGIN_INSTALL_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/LV2" CACHE STRING
    "Install destination for LV2 bundle [default: $ENV{HOME}/Library/Audio/Plug-Ins/LV2]")
elseif(MSVC)
    set(LV2_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lv2" CACHE STRING
    "Install destination for LV2 bundle [default: ${CMAKE_INSTALL_PREFIX}/lv2]")
else()
    set(LV2_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/lv2" CACHE STRING
    "Install destination for LV2 bundle [default: ${CMAKE_INSTALL_PREFIX}/lib/lv2]")
endif()

include(StringUtility)

function(generate_lv2_controllers_ttl FILE)
    file(WRITE "${FILE}" "# LV2 parameters for MIDI CC controllers
@prefix atom:   <http://lv2plug.in/ns/ext/atom#> .
@prefix lv2:    <http://lv2plug.in/ns/lv2core#> .
@prefix midi:   <http://lv2plug.in/ns/ext/midi#> .
@prefix patch:  <http://lv2plug.in/ns/ext/patch#> .
@prefix rdfs:   <http://www.w3.org/2000/01/rdf-schema#> .
@prefix ${PROJECT_NAME}:  <${LV2_PLUGIN_URI}#> .
")
    math(EXPR _j "${MIDI_CC_COUNT}-1")
    foreach(_i RANGE "${_j}")
        if(_i LESS 128 AND PLUGIN_LV2_PSA)
            continue() # Don't generate automation parameters for CCs with plugin-side automation
        endif()

        string_left_pad(_i "${_i}" 3 0)
        file(APPEND "${FILE}" "
${PROJECT_NAME}:cc${_i}
  a lv2:Parameter ;
  rdfs:label \"Controller ${_i}\" ;
  rdfs:range atom:Float ;
  lv2:minimum 0.0 ;
  lv2:maximum 1.0")

        if(_i LESS 128 AND NOT PLUGIN_LV2_PSA)
            math(EXPR _digit1 "${_i}>>4")
            math(EXPR _digit2 "${_i}&15")
            string(SUBSTRING "0123456789ABCDEF" "${_digit1}" 1 _digit1)
            string(SUBSTRING "0123456789ABCDEF" "${_digit2}" 1 _digit2)
            file(APPEND "${FILE}" " ;
  midi:binding \"B0${_digit1}${_digit2}00\"^^midi:MidiEvent .
")
        else()
            file(APPEND "${FILE}" " .
")
        endif()
    endforeach()

    file(APPEND "${FILE}" "
<${LV2_PLUGIN_URI}>
  a lv2:Plugin ;
")

    file(APPEND "${FILE}" "  patch:readable")
    if(NOT PLUGIN_LV2_PSA)
        file(APPEND "${FILE}" " ${PROJECT_NAME}:cc000")
        foreach(_i RANGE 1 "${_j}")
            string_left_pad(_i "${_i}" 3 0)
            file(APPEND "${FILE}" ", ${PROJECT_NAME}:cc${_i}")
        endforeach()
    else()
        file(APPEND "${FILE}" " ${PROJECT_NAME}:cc128")
        foreach(_i RANGE 129 "${_j}")
            string_left_pad(_i "${_i}" 3 0)
            file(APPEND "${FILE}" ", ${PROJECT_NAME}:cc${_i}")
        endforeach()
    endif()

    file(APPEND "${FILE}" " ;
")

    file(APPEND "${FILE}" "  patch:writable")
    if(NOT PLUGIN_LV2_PSA)
        file(APPEND "${FILE}" " ${PROJECT_NAME}:cc000")
        foreach(_i RANGE 1 "${_j}")
            string_left_pad(_i "${_i}" 3 0)
            file(APPEND "${FILE}" ", ${PROJECT_NAME}:cc${_i}")
        endforeach()
    else()
        file(APPEND "${FILE}" " ${PROJECT_NAME}:cc128")
        foreach(_i RANGE 129 "${_j}")
            string_left_pad(_i "${_i}" 3 0)
            file(APPEND "${FILE}" ", ${PROJECT_NAME}:cc${_i}")
        endforeach()
    endif()
    file(APPEND "${FILE}" " .
")
endfunction()
