cmake_minimum_required(VERSION 3.10)

# Project name and version
project(play_file_tilde VERSION 1.0)

# Add the pd-cmake module
include(${CMAKE_SOURCE_DIR}/pd.cmake)

# Set the library name
set(LIB_NAME "play_file_tilde")

# External directories
set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-7.0.1")
set(FFMPEG_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg-7.0.1")
set(LIBSAMPLERATE_DIR "${CMAKE_SOURCE_DIR}/../../../Shared/libsamplerate")

# Set FFMPEG libraries
set(FFMPEG_LIBS
    "${FFMPEG_OUT_DIR}/libavcodec/libavcodec.a"
    "${FFMPEG_OUT_DIR}/libavformat/libavformat.a"
    "${FFMPEG_OUT_DIR}/libavutil/libavutil.a"
    "${FFMPEG_OUT_DIR}/libswresample/libswresample.a"
)

# Add the source files
set(SOURCES
    play.file~.c
    sfload.c
)

# Create the Pure Data external
add_pd_external(${LIB_NAME} ${LIB_NAME} ${SOURCES})

# Set compiler flags
target_include_directories(${LIB_NAME} PRIVATE ${LIBSAMPLERATE_DIR} ${FFMPEG_OUT_DIR})
target_compile_options(${LIB_NAME} PRIVATE -DHAVE_STRUCT_TIMESPEC)

# Link the FFMPEG libraries
target_link_libraries(${LIB_NAME} PRIVATE
    ${FFMPEG_LIBS}
    -lz
)

# Platform-specific configurations
if(APPLE)
    target_compile_options(${LIB_NAME} PRIVATE -mmacosx-version-min=10.9)
    set(FFMPEG_CONFIG_FLAGS "--extra-cflags=-mmacosx-version-min=10.9 --extra-ldflags=-mmacosx-version-min=10.9")
    #set(FFMPEG_CC "clang -arch x86_64 -arch arm64")
elseif(UNIX AND NOT APPLE)
    set(FFMPEG_CONFIG_FLAGS "--enable-pic")
    set(FFMPEG_CC "${CMAKE_C_COMPILER}")
elseif(WIN32)
    target_link_libraries(${LIB_NAME} PRIVATE -lbcrypt)
    set(FFMPEG_CC "${CMAKE_C_COMPILER}")
endif()

# Handle architecture-specific settings
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" AND DEFINED extension AND extension STREQUAL "d_arm64")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()

execute_process(
    COMMAND tar xjf ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-7.0.1.tar.bz2 -C ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(ffmpeg
    COMMAND bash -c "./configure --disable-asm --cc=clang --enable-static --disable-shared --enable-optimizations --disable-debug --disable-doc --disable-programs --disable-iconv --disable-avdevice --disable-postproc --disable-network --disable-everything --enable-avcodec --enable-avformat --enable-avutil --enable-swscale --enable-swresample --enable-decoder=mp3*,pcm*,aac*,flac,vorbis,opus --enable-parser=mpegaudio,aac --enable-demuxer=mp3,wav,aiff,flac,aac,ogg,pcm* --enable-filter=aresample --enable-protocol=file ${FFMPEG_CONFIG_FLAGS}"
    COMMAND bash -c "make"
    WORKING_DIRECTORY ${FFMPEG_OUT_DIR}
)

add_dependencies(${LIB_NAME} ffmpeg)

# Clean up
add_custom_target(clean_ffmpeg
    COMMAND rm -rf ${FFMPEG_DIR}
    COMMAND rm -f *.d_*
    COMMAND rm -f *.pd_*
)
