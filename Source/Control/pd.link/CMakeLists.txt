cmake_minimum_required(VERSION 3.10)
project(pd_link C CXX)

message(STATUS "Configuring Opus")
set(MESSAGE_QUIET ON)
add_subdirectory(opus)
set(MESSAGE_QUIET OFF)

# Define sources for common files
set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/udp/udp_discovery_peer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/udp/udp_discovery_protocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/link.cpp
)

add_library(pdlink_common STATIC ${COMMON_SOURCES})

# Platform-specific configurations
if(WIN32)
    target_link_options(pdlink_common PUBLIC -static-libgcc -static-libstdc++ -static)
    target_link_libraries(pdlink_common PUBLIC "ws2_32;iphlpapi;stdc++")
endif()

if(WIN32)
target_compile_definitions(opus PRIVATE FLOAT_APPROX=1 _POSIX_SEM_VALUE_MAX=32767)
target_compile_options(opus PRIVATE -msse2)
target_compile_definitions(pdlink_common PUBLIC _POSIX_SEM_VALUE_MAX=32767)
endif()

add_pd_external(pdlink pd.link pd.link.c)
add_pd_external(pdlink_tilde pd.link~ pd.link~.c)

target_include_directories(pdlink_tilde PRIVATE ./opus/include ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared/libsamplerate)

target_link_libraries(pdlink PRIVATE ${LINK_LIBS} else_shared pdlink_common)
target_link_libraries(pdlink_tilde PRIVATE ${LINK_LIBS} else_shared opus pdlink_common)
