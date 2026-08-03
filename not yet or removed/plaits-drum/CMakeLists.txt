cmake_minimum_required(VERSION 3.10)

# Header-only now (everything got merged into single .h files, no .cc left
# for this target), so this is an INTERFACE library instead of a STATIC one.
# INTERFACE libraries don't compile anything themselves - they just carry
# include dirs / defines / flags to whatever links against them.
add_library(bassdrum INTERFACE)

target_compile_definitions(bassdrum INTERFACE TEST=1)
target_compile_options(bassdrum INTERFACE -Wno-unused-local-typedefs)
target_include_directories(bassdrum INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_link_options(bassdrum INTERFACE -static-libgcc -static-libstdc++ -static)
    target_link_libraries(bassdrum INTERFACE "ws2_32;stdc++")
endif()

# Create the Pure Data external
pd_add_external(bassdrum~ bassdrum~.cpp)

target_link_libraries(bassdrum_tilde PRIVATE bassdrum)