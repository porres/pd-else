cmake_minimum_required(VERSION 3.10)
project(sfont~ C)

# Set the library name
set(LIB_NAME "sfont_tilde")

# Define the source files
set(FLUID_SOURCES
    FluidLite/src/fluid_init.c
    FluidLite/src/fluid_chan.c
    FluidLite/src/fluid_chorus.c
    FluidLite/src/fluid_conv.c
    FluidLite/src/fluid_defsfont.c
    FluidLite/src/fluid_dsp_float.c
    FluidLite/src/fluid_gen.c
    FluidLite/src/fluid_hash.c
    FluidLite/src/fluid_list.c
    FluidLite/src/fluid_mod.c
    FluidLite/src/fluid_ramsfont.c
    FluidLite/src/fluid_rev.c
    FluidLite/src/fluid_settings.c
    FluidLite/src/fluid_synth.c
    FluidLite/src/fluid_sys.c
    FluidLite/src/fluid_tuning.c
    FluidLite/src/fluid_voice.c
)

set(VORBIS_SOURCES
    FluidLite/libvorbis-1.3.5/lib/vorbisenc.c
    FluidLite/libvorbis-1.3.5/lib/info.c
    FluidLite/libvorbis-1.3.5/lib/analysis.c
    FluidLite/libvorbis-1.3.5/lib/bitrate.c
    FluidLite/libvorbis-1.3.5/lib/block.c
    FluidLite/libvorbis-1.3.5/lib/codebook.c
    FluidLite/libvorbis-1.3.5/lib/envelope.c
    FluidLite/libvorbis-1.3.5/lib/floor0.c
    FluidLite/libvorbis-1.3.5/lib/floor1.c
    FluidLite/libvorbis-1.3.5/lib/lookup.c
    FluidLite/libvorbis-1.3.5/lib/lpc.c
    FluidLite/libvorbis-1.3.5/lib/lsp.c
    FluidLite/libvorbis-1.3.5/lib/mapping0.c
    FluidLite/libvorbis-1.3.5/lib/mdct.c
    FluidLite/libvorbis-1.3.5/lib/psy.c
    FluidLite/libvorbis-1.3.5/lib/registry.c
    FluidLite/libvorbis-1.3.5/lib/res0.c
    FluidLite/libvorbis-1.3.5/lib/sharedbook.c
    FluidLite/libvorbis-1.3.5/lib/smallft.c
    FluidLite/libvorbis-1.3.5/lib/vorbisfile.c
    FluidLite/libvorbis-1.3.5/lib/window.c
    FluidLite/libvorbis-1.3.5/lib/synthesis.c
)

set(OGG_SOURCES
    FluidLite/libogg-1.3.2/src/bitwise.c
    FluidLite/libogg-1.3.2/src/framing.c
)

set(EXTRA_SOURCES
    ../../../Shared/elsefile.c
)

# Combine all sources
set(SOURCES
    sfont~.c
    ${EXTRA_SOURCES}
    ${FLUID_SOURCES}
    ${VORBIS_SOURCES}
    ${OGG_SOURCES}
)

# Include directories
include_directories(
    ../../
    FluidLite/libvorbis-1.3.5/include
    FluidLite/libvorbis-1.3.5/lib
    FluidLite/libogg-1.3.2/include
    FluidLite/include
)

# Set compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_STRUCT_TIMESPEC ${EXTRA_INCLUDES}")

# Check for architecture-specific flags
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm64" AND ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()

# Linker flags
set(CMAKE_SHARED_LINKER_FLAGS "${EXTRA_LDFLAGS}")

# Add the Pure Data external library
add_pd_external(${LIB_NAME} ${LIB_NAME} ${SOURCES})

# Install data files and directories (if needed)
# Uncomment and adjust if there are data files or directories to install
# install(FILES sfont~-help.pd DESTINATION pd/extra/${LIB_NAME})
# install(DIRECTORY sf DESTINATION pd/extra/${LIB_NAME})
