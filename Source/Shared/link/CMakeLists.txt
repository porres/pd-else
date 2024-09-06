# Define sources for common files
set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/udp/udp_discovery_peer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/udp/udp_discovery_protocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/link.cpp
)

add_library(link STATIC ${COMMON_SOURCES})
target_include_directories(link PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_link_options(link PUBLIC -static-libgcc -static-libstdc++ -static)
    target_link_libraries(link PUBLIC "ws2_32;iphlpapi;stdc++")
    target_compile_definitions(link PUBLIC _POSIX_SEM_VALUE_MAX=32767)
endif()
