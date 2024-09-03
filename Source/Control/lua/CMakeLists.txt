cmake_minimum_required(VERSION 3.10)
project(pdlua C)

# Get the version from git
execute_process(
    COMMAND git describe --tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE git_result
    OUTPUT_VARIABLE pdlua_version
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT git_result EQUAL 0)
    set(pdlua_version "unknown")
endif()

add_library(lua STATIC ${CMAKE_CURRENT_SOURCE_DIR}/lua/onelua.c)
target_include_directories(lua PUBLIC lua)

if(APPLE)
    target_compile_definitions(lua PUBLIC LUA_USE_MACOSX=1 MAKE_LIB=1)
elseif(UNIX)
    target_compile_definitions(lua PUBLIC LUA_USE_LINUX=1 MAKE_LIB=1)
elseif(WIN32)
    target_compile_definitions(lua PUBLIC LUA_USE_WINDOWS=1 MAKE_LIB=1)
endif()

# Define the external
add_pd_external(pdlua pdlua pdlua.c)

# Libraries to link against
target_link_libraries(pdlua PRIVATE lua)
target_include_directories(pdlua PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lua)
target_compile_definitions(pdlua PRIVATE PDLUA_VERSION=${pdlua_version})
