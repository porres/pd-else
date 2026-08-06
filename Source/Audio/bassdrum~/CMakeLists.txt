cmake_minimum_required(VERSION 3.10)

# Add the source files
set(BASSDRUM_SOURCES
    stmlib/utils/random.cc
    plaits/dsp/voice.cc
    plaits/dsp/engine/hi_hat_engine.cc
    plaits/dsp/engine/snare_drum_engine.cc
    plaits/resources.cc
)

add_library(bassdrum STATIC ${BASSDRUM_SOURCES})

target_compile_definitions(bassdrum PUBLIC TEST=1)
target_compile_options(bassdrum PRIVATE -Wno-unused-local-typedefs)
target_include_directories(bassdrum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_link_options(bassdrum PUBLIC -static-libgcc -static-libstdc++ -static)
    target_link_libraries(bassdrum PUBLIC "ws2_32;stdc++")
endif()

# Create the Pure Data external
pd_add_external(bassdrum~ bassdrum~.cpp)

target_link_libraries(bassdrum_tilde PRIVATE bassdrum)