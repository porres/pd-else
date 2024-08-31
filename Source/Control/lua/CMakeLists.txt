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

set(LUA_FLAGS "-DMAKE_LIB -Ilua")

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LUA_FLAGS "${LUA_FLAGS} -DLUA_USE_MACOSX")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LUA_FLAGS "${LUA_FLAGS} -DLUA_USE_LINUX")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LUA_FLAGS "${LUA_FLAGS} -DLUA_USE_WINDOWS")
endif()

add_library(lua STATIC ${CMAKE_CURRENT_SOURCE_DIR}/lua/onelua.c)

# Define the external
add_pd_external(pdlua pdlua pdlua.c)

# Libraries to link against
target_link_libraries(pdlua PRIVATE lua)
target_include_directories(pdlua PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lua)
target_compile_options(lua PUBLIC ${LUA_FLAGS})
target_compile_definitions(pdlua PRIVATE PDLUA_VERSION=${pdlua_version})
