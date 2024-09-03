cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)

project(ELSE LANGUAGES C CXX)

set(PD_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/else" CACHE STRING "Externals binary path")

include(${CMAKE_CURRENT_SOURCE_DIR}/pd.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/shared.cmake)

function(message)
    if (NOT MESSAGE_QUIET)
        _message(${ARGN})
    endif()
endfunction()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Default build type: Release" FORCE)
endif()

file(GLOB else_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Control/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Audio/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Extra/Aliases/*.c
)

file(GLOB aubio_sources
   ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/aubio/src/**/*.c
   ${CMAKE_CURRENT_SOURCE_DIR}/Source/shared/aubio/src/*.c
)

include_directories(${PD_SOURCES_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/aubio/src)

foreach(file ${else_sources})
    get_filename_component(name ${file} NAME)
    string(REGEX REPLACE "\\.[^.]*$" "" name_we ${name})
    string(REGEX REPLACE "~" "_tilde" target_name ${name_we})
    add_pd_external(${target_name} ${name_we} ${file})
    target_link_libraries(${target_name} PRIVATE else_shared)
endforeach()

target_sources(beat_tilde PRIVATE ${aubio_sources})

add_subdirectory(Source/Control/pd.link)
add_subdirectory(Source/Audio/circuit~)
add_subdirectory(Source/Audio/plaits~)
add_subdirectory(Source/Audio/play.file~)
add_subdirectory(Source/Audio/sfont~)
add_subdirectory(Source/Audio/sfz~)
add_subdirectory(Source/Control/lua)

target_link_libraries(else PRIVATE pdlua)

set(pdf_help "${CMAKE_CURRENT_SOURCE_DIR}/Documentation/README.pdf")
file(GLOB help_files "${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Help-files/*")
file(GLOB extra_files
    ${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Extra-files/*
    ${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Extra-files/audio/*
)

file(GLOB merda_files "${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Merda/Modules/*")
file(GLOB abstractions ${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Audio/* ${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Control/* ${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Extra/*)
file(MAKE_DIRECTORY ${else})
file(COPY ${pdf_help} ${extra_files} ${help_files} ${abstractions} DESTINATION ${PD_OUTPUT_PATH})
file(COPY ${merda_files} DESTINATION ${PD_OUTPUT_PATH}/Merda)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Source/Audio/sfz~/sfz" DESTINATION ${PD_OUTPUT_PATH}/sfz)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Live-Electronics-Tutorial" DESTINATION ${PD_OUTPUT_PATH}/Live-Electronics-Tutorial)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" DESTINATION ${PD_OUTPUT_PATH}/LICENSE)
file(REMOVE_RECURSE ${PD_OUTPUT_PATH}/audio)
