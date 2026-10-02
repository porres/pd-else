cmake_minimum_required(VERSION 3.10)

# Create the Pure Data externals
pd_add_external(bassdrum~ bassdrum~.cpp)
pd_add_external(bd~ bassdrum~.cpp)

foreach(tgt bassdrum_tilde bd_tilde)
    target_compile_definitions(${tgt} PRIVATE TEST=1 _USE_MATH_DEFINES=1)
    target_compile_features(${tgt} PRIVATE cxx_std_17)
endforeach()

if(WIN32)
    foreach(tgt bassdrum_tilde bd_tilde)
        target_link_options(${tgt} PRIVATE -static-libgcc -static-libstdc++ -static)
        target_link_libraries(${tgt} PRIVATE "ws2_32;stdc++")
    endforeach()
endif()