cmake_minimum_required(VERSION 3.15)

set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_MACOSX_RPATH Off)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(pd-circuit)

add_pd_external(pd-circuit circuit~ ${CMAKE_CURRENT_SOURCE_DIR}/Source/circuit~.c)

file(GLOB KLU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/KLU/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/COLAMD/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/AMD/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/BTF/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/SuiteSparse_config/*.c")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Libraries/")

file(GLOB SAMPLERATE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../../Source/Shared/libsamplerate/*.c")
add_library(samplerate STATIC ${SAMPLERATE_SOURCES})
add_library(klu STATIC ${KLU_SOURCES})

add_library(circuit-simulator STATIC ${CMAKE_CURRENT_SOURCE_DIR}/Source/Simulator.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Source/Simulator.h ${CMAKE_CURRENT_SOURCE_DIR}/Source/Netlist.h ${CMAKE_CURRENT_SOURCE_DIR}/Source/Components.h)
target_include_directories(circuit-simulator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries)
target_link_libraries(pd-circuit samplerate klu circuit-simulator)
