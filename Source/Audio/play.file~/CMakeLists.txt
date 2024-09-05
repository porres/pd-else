cmake_minimum_required(VERSION 3.10)

# Project name and version
project(play_file VERSION 1.0)

# Add the pd-cmake module
include(${CMAKE_SOURCE_DIR}/pd.cmake)

# External directories
set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-7.0.1")
set(FFMPEG_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg-7.0.1")
set(LIBSAMPLERATE_DIR "${CMAKE_SOURCE_DIR}/../../../Shared/libsamplerate")

set(FFMPEG_LIBS
    "${FFMPEG_OUT_DIR}/libavformat/libavformat.a"
    "${FFMPEG_OUT_DIR}/libavcodec/libavcodec.a"
    "${FFMPEG_OUT_DIR}/libavutil/libavutil.a"
    "${FFMPEG_OUT_DIR}/libswresample/libswresample.a"
)

message(STATUS "Configuring ffmpeg")
execute_process(
    COMMAND tar xjf ./ffmpeg-7.0.1.tar.bz2 -C ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(
    OUTPUT ${FFMPEG_LIBS}
    COMMAND bash -c "${CMAKE_CURRENT_SOURCE_DIR}/build_ffmpeg.sh ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_C_COMPILER_LAUNCHER}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Building FFmpeg libraries..."
    VERBATIM
)

# Define the custom target that depends on the FFmpeg build
add_custom_target(
    ffmpeg
    ALL
    DEPENDS ${FFMPEG_LIBS}
)

# Create the Pure Data external
add_pd_external(play_file_tilde play.file~ play.file~.c)
add_pd_external(sfload sfload sfload.c)

# Set compiler flags
target_include_directories(play_file_tilde PRIVATE ${LIBSAMPLERATE_DIR} ${FFMPEG_OUT_DIR})
target_compile_options(play_file_tilde PRIVATE -DHAVE_STRUCT_TIMESPEC)
target_link_libraries(play_file_tilde PRIVATE ${FFMPEG_LIBS} z)

target_include_directories(sfload PRIVATE ${LIBSAMPLERATE_DIR} ${FFMPEG_OUT_DIR})
target_compile_options(sfload PRIVATE -DHAVE_STRUCT_TIMESPEC)
target_link_libraries(sfload PRIVATE ${FFMPEG_LIBS} z)

# Platform-specific configurations
if(APPLE)
    set(FFMPEG_CONFIG_FLAGS "--extra-cflags=-mmacosx-version-min=10.9 --extra-ldflags=-mmacosx-version-min=10.9")
    set(FFMPEG_CC "${CMAKE_C_COMPILER} -arch x86_64 -arch arm64")
elseif(UNIX AND NOT APPLE)
    set(FFMPEG_CONFIG_FLAGS --enable-pic)
    set(FFMPEG_CC "${CMAKE_C_COMPILER}")
elseif(WIN32)
    target_link_libraries(play_file_tilde PRIVATE -lbcrypt)
    target_link_libraries(sfload PRIVATE -lbcrypt)
    set(FFMPEG_CC "${CMAKE_C_COMPILER}")
endif()

add_dependencies(play_file_tilde ffmpeg)
add_dependencies(sfload ffmpeg)

# Clean up
add_custom_target(clean_ffmpeg
    COMMAND rm -rf ${FFMPEG_DIR}
    COMMAND rm -f *.d_*
    COMMAND rm -f *.pd_*
)
