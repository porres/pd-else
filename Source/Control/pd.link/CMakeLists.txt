cmake_minimum_required(VERSION 3.10)
project(pd_link C CXX)

# Set the library name
set(LIB_NAME "pdlink")

# Define sources for Opus
file(GLOB OPUS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/analysis.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/extensions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/mapping_matrix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/mlp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/mlp_data.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/opus.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/opus_compare.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/opus_decoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/opus_encoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/opus_multistream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/opus_multistream_decoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/opus_multistream_encoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/opus_projection_decoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/opus_projection_encoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/src/repacketizer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/bands.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/celt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/celt_decoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/celt_encoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/celt_lpc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/cwrs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/entcode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/entdec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/entenc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/kiss_fft.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/laplace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/mathops.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/mdct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/modes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/pitch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/quant_bands.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/rate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/celt/vq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/silk/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opus/silk/float/*.c
)

# Define sources for common files
set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/udp/udp_discovery_peer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/udp/udp_discovery_protocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/link.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../Shared/magic.c
)

# Define sources for the library
file(GLOB SIGNAL_SOURCES
    ${OPUS_SOURCES}
    ../../../Shared/libsamplerate/*.c
)

# Compiler flags
set(OPUS_FLAGS "-DUSE_ALLOCA=1 -DOPUS_BUILD=1 -DFLOAT_APPROX=1")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OPUS_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -I../../../Shared -I../../../Shared/libsamplerate")

# Platform-specific configurations
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LINK_LIBS "${LINK_LIBS} ws2_32;iphlpapi;stdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
endif()

add_pd_external(pdlink pd.link pd.link.c ${COMMON_SOURCES})
add_pd_external(pdlink_tilde pd.link~ pd.link~.c ${COMMON_SOURCES} ${SIGNAL_SOURCES})

target_include_directories(pdlink_tilde PRIVATE ./opus/include ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared/libsamplerate)

target_link_libraries(pdlink PRIVATE ${LINK_LIBS})
target_link_libraries(pdlink_tilde PRIVATE ${LINK_LIBS})
