cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

project(ELSE LANGUAGES C CXX)

set(PD_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/else" CACHE STRING "Externals binary path")

include(pd.cmake)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Default build type: Release" FORCE)
endif()

file(GLOB else_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/control/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/audio/*.c
)

file(GLOB shared_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/*.c
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared)

foreach(file ${else_sources})
    get_filename_component(name ${file} NAME)
    string(REGEX REPLACE "\\.[^.]*$" "" name_we ${name})
    string(REGEX REPLACE "~" "_tilde" name_we ${name_we})
    add_pd_external(${name_we}_obj ${name_we} ${file} ${shared_sources})
endforeach()

add_subdirectory(Source/audio/circuit~)
add_subdirectory(Source/audio/plaits~)
add_subdirectory(Source/audio/play.file~)
add_subdirectory(Source/audio/sfont~)
add_subdirectory(Source/audio/sfz~)
add_subdirectory(Source/control/lua)
add_subdirectory(Source/control/pd.link)

file(GLOB help_files "${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Help-files/*")
file(GLOB extra_files "${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Extra-files/*")
file(GLOB merda_files "${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Merda/Modules/*")

file(MAKE_DIRECTORY ${else})
file(COPY ${extra_files} ${help_files} DESTINATION ${PD_OUTPUT_PATH})
file(COPY ${merda_files} DESTINATION ${PD_OUTPUT_PATH}/Merda)
