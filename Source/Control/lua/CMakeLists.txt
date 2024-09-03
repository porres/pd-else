cmake_minimum_required(VERSION 3.10)
project(pdlua C)

# Define the external
add_pd_external(pdlua pdlua pdlua.c)
target_sources(pdlua PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lua/onelua.c)

target_include_directories(pdlua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lua)
if(APPLE)
    target_compile_definitions(pdlua PRIVATE LUA_USE_MACOSX=1 MAKE_LIB=1 PDLUA_VERSION="1.0.0")
elseif(UNIX)
    target_compile_definitions(pdlua PRIVATE LUA_USE_LINUX=1 MAKE_LIB=1 PDLUA_VERSION="1.0.0")
elseif(WIN32)
    target_compile_definitions(pdlua PRIVATE LUA_USE_WINDOWS=1 MAKE_LIB=1 PDLUA_VERSION="1.0.0")
endif()

if(WIN32)
    target_link_options(pdlua PUBLIC -static-libgcc -static)
    target_link_libraries(pdlua PUBLIC "ws2_32;")
endif()
