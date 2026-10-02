cmake_minimum_required(VERSION 3.10)

pd_add_external(bassdrum~ bassdrum~.c)
pd_add_external(bd~ bassdrum~.c)

foreach(tgt bassdrum_tilde bd_tilde)
    target_sources(${tgt} PRIVATE bass_drum.c)
    target_compile_definitions(${tgt} PRIVATE _USE_MATH_DEFINES=1)
endforeach()

if(WIN32)
    foreach(tgt bassdrum_tilde bd_tilde)
        target_link_options(${tgt} PRIVATE -static-libgcc -static)
        target_link_libraries(${tgt} PRIVATE "ws2_32")
    endforeach()
endif()