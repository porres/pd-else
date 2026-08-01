cmake_minimum_required(VERSION 3.10)

# Add the source files
set(DRUM_SOURCES
    stmlib/dsp/units.cc
    stmlib/utils/random.cc
    stmlib/dsp/atan.cc
    plaits/dsp/voice.cc
    plaits/dsp/engine/bass_drum_engine.cc
    plaits/dsp/engine/hi_hat_engine.cc
    plaits/dsp/engine/snare_drum_engine.cc
    plaits/resources.cc
)

add_library(drum STATIC ${DRUM_SOURCES})

target_compile_definitions(drum PUBLIC TEST=1 _USE_MATH_DEFINES=1)
target_compile_options(drum PRIVATE -Wno-unused-local-typedefs)
target_include_directories(drum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_link_options(drum PUBLIC -static-libgcc -static-libstdc++ -static)
    target_link_libraries(drum PUBLIC "ws2_32;stdc++")
endif()

# Create the Pure Data external
pd_add_external(drum~ drum~.cpp)

target_link_libraries(drum_tilde PRIVATE drum)