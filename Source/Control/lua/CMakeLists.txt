cmake_minimum_required(VERSION 3.10)
project(pdlua C)

# Set the library name
set(LIB_NAME "pdlua")

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

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LUA_FLAGS} -DPDLUA_VERSION=\"${pdlua_version}\"")

# Source files
set(SOURCES
    pdlua.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lua/onelua.c
)

# Define the external
add_pd_external(${LIB_NAME} ${LIB_NAME} ${SOURCES})

# Libraries to link against
target_link_libraries(${LIB_NAME} PRIVATE ${LUA_LIBS})
target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lua)
