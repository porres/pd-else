# External directories
set(FFMPEG_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg-7.0.1")

set(FFMPEG_LIBS
    "${FFMPEG_OUT_DIR}/libavformat/libavformat.a"
    "${FFMPEG_OUT_DIR}/libavcodec/libavcodec.a"
    "${FFMPEG_OUT_DIR}/libavutil/libavutil.a"
    "${FFMPEG_OUT_DIR}/libswresample/libswresample.a"
)

message(STATUS "Configuring ffmpeg")
file(MAKE_DIRECTORY ${FFMPEG_OUT_DIR})
string(REGEX REPLACE "^([A-Za-z]):" "/\\1" FFMPEG_TAR_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-7.0.1.tar.bz2")
execute_process(
    COMMAND tar xjf ${FFMPEG_TAR_PATH} -C ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
file(REMOVE ${FFMPEG_OUT_DIR}/VERSION) # Causes problems, because there is some header somewhere named just <version>

add_custom_command(
    OUTPUT ${FFMPEG_LIBS}
    COMMAND bash -c "${CMAKE_CURRENT_SOURCE_DIR}/build_ffmpeg.sh ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_C_COMPILER_LAUNCHER}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Building FFmpeg libraries..."
    VERBATIM
)

# Define the custom target that depends on the FFmpeg build
add_custom_target(
    ffmpeg_deps
    ALL
    DEPENDS ${FFMPEG_LIBS}
)

add_library(ffmpeg INTERFACE)
target_link_libraries(ffmpeg INTERFACE ${FFMPEG_LIBS} z)
target_include_directories(ffmpeg INTERFACE ${FFMPEG_OUT_DIR})
add_dependencies(ffmpeg ffmpeg_deps)

if(WIN32)
    target_link_libraries(ffmpeg INTERFACE bcrypt ws2_32)
endif()

# Clean up
add_custom_target(clean_ffmpeg
    COMMAND rm -rf ${FFMPEG_DIR}
    COMMAND rm -f *.d_*
    COMMAND rm -f *.pd_*
)
