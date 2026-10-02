cmake_minimum_required(VERSION 3.10)

# Same layout as the plaits~ CMakeLists: the DSP sources are built into a static
# library which the external links against. Only the files bassdrum~ needs.
set(BASSDRUM_SOURCES
   dsp.cc
   voice.cc
   resources.cc
)

add_library(bassdrum STATIC ${BASSDRUM_SOURCES})
target_compile_definitions(bassdrum PUBLIC TEST=1 _USE_MATH_DEFINES=1)
target_compile_features(bassdrum PUBLIC cxx_std_14)
target_compile_options(bassdrum PRIVATE -Wno-unused-local-typedefs)
target_include_directories(bassdrum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(bassdrum PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(WIN32)
    target_link_options(bassdrum PUBLIC -static-libgcc -static-libstdc++ -static)
    target_link_libraries(bassdrum PUBLIC "ws2_32;stdc++")
endif()

# Create the Pure Data external
pd_add_external(bassdrum~ bassdrum~.cpp)
target_link_libraries(bassdrum_tilde PRIVATE bassdrum)

# bd~ is an alias for bassdrum~, same source, different class name.
pd_add_external(bd~ bassdrum~.cpp)
target_link_libraries(bd_tilde PRIVATE bassdrum)

# bassdrum~ contains its own copy of the plaits/stmlib code (same namespaces and
# symbol names as plaits~, but a different Voice). Compile the DSP library with
# hidden symbols so they stay private to this binary and Pd loading both
# externals can never bind one to the other. The external itself is left at
# default visibility, so bassdrum_tilde_setup is always exported.
if(NOT WIN32)
    set_target_properties(bassdrum PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endif()