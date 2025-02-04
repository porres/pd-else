cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_MACOSX_RPATH ON)

option(ENABLE_OPUS "Enable opus for pdlink~ stream compression" ON)
if(!ENABLE_OPUS)
  add_compile_definitions(ENABLE_OPUS=0)
endif()

if(UNIX AND NOT APPLE)
    set(CMAKE_BUILD_RPATH "$ORIGIN")
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()
if(APPLE)
    set(CMAKE_BUILD_RPATH "@loader_path")
    set(CMAKE_INSTALL_RPATH "@loader_path")
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)

project(ELSE LANGUAGES C CXX)

set(PD_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/else" CACHE STRING "Externals binary path")

include(${CMAKE_CURRENT_SOURCE_DIR}/pd.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/shared.cmake)

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
   ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/aubio/src/*.c
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
target_link_libraries(else PRIVATE pdlua)

add_subdirectory(Source/Audio/circuit~)
add_subdirectory(Source/Audio/plaits~)
add_subdirectory(Source/Audio/sfont~)
add_subdirectory(Source/Audio/sfz~)
add_subdirectory(Source/Control/lua)

add_subdirectory(Source/Shared/link)
add_subdirectory(Source/Shared/ffmpeg)

if(WIN32)
    target_compile_definitions(opus PUBLIC FLOAT_APPROX=1 _POSIX_SEM_VALUE_MAX=32767)
    target_compile_options(opus PRIVATE -msse2)
endif()

target_link_libraries(play.file_tilde PRIVATE ffmpeg)
target_link_libraries(sfload PRIVATE ffmpeg)
target_link_libraries(sfinfo PRIVATE ffmpeg)

target_link_libraries(pdlink PUBLIC link)
target_link_libraries(pdlink_tilde PUBLIC link opus)
target_include_directories(pdlink_tilde PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/libsamplerate ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/opus)

set(pdf_help "${CMAKE_CURRENT_SOURCE_DIR}/Documentation/README.pdf")
file(GLOB help_files "${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Help-files/*")
file(GLOB extra_files
    ${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Extra-files/*
    ${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Extra-files/audio/*
)

set(lua_resources
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Audio/scope3d~.pd_lua
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Control/lua/pd.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Control/lua/pdx.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Control/lua/luadoc/hello.lua
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Control/lua/luadoc/hello.pd_lua
)

file(GLOB merda_files "${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Merda/Modules/*")
file(GLOB tcl_files "${CMAKE_CURRENT_SOURCE_DIR}/Source/Extra/*.tcl")
file(GLOB abstractions ${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Audio/*.pd ${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Control/*.pd ${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Extra/*)
file(COPY ${pdf_help} ${extra_files} ${help_files} ${abstractions} ${tcl_files} ${lua_resources} DESTINATION ${PD_OUTPUT_PATH})
file(COPY ${merda_files} DESTINATION ${PD_OUTPUT_PATH})
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Source/Audio/sfz~/sfz" DESTINATION ${PD_OUTPUT_PATH})
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Live-Electronics-Tutorial" DESTINATION ${PD_OUTPUT_PATH})
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" DESTINATION ${PD_OUTPUT_PATH}/LICENSE)
file(REMOVE_RECURSE ${PD_OUTPUT_PATH}/audio)

# Log the object count
function(count_files_and_directories dir extension file_count count_dirs)
    file(GLOB files "${dir}/*")
    foreach(file ${files})
        get_filename_component(file_name "${file}" NAME)
        if(NOT file_name MATCHES "^\\."
           AND NOT file_name MATCHES "-help\\.pd$"
           AND NOT file_name MATCHES ".unit\\.pd$"
           AND NOT file_name MATCHES "about.MERDA.pd"
           AND NOT (IS_DIRECTORY ${file} AND NOT count_dirs))  # Exclude hidden files and help files
            math(EXPR ${file_count} "${${file_count}} + 1")
        endif()
    endforeach()
    set(${file_count} ${${file_count}} PARENT_SCOPE)
endfunction()

count_files_and_directories("${CMAKE_CURRENT_SOURCE_DIR}/Source/Control" "c" control_c_file_count YES)
count_files_and_directories("${CMAKE_CURRENT_SOURCE_DIR}/Source/Audio" "c" signal_c_file_count YES)
count_files_and_directories("${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Control" "pd" control_pd_file_count YES)
count_files_and_directories("${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Audio" "pd" signal_pd_file_count YES)
count_files_and_directories("${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Merda/Modules" "pd" modules_pd_file_count NO)
math(EXPR total_compiled "${control_c_file_count} + ${signal_c_file_count}")
math(EXPR total_abstractions "${control_pd_file_count} + ${signal_pd_file_count}")

message(STATUS "${total_compiled} coded objects (${signal_c_file_count} signal objects / ${control_c_file_count} control objects)")
message(STATUS "${total_abstractions} abstractions (${signal_pd_file_count} signal objects / ${control_pd_file_count} control objects)")
message(STATUS "Merda modules: ${modules_pd_file_count}")
