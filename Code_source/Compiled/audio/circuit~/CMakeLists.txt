# Define your standard CMake header (for example):
cmake_minimum_required(VERSION 2.8)

set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_MACOSX_RPATH Off)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include pd.cmake (1):  
include(${CMAKE_CURRENT_SOURCE_DIR}/Libraries/pd.build/pd.cmake)

# Declare the name of the project:   
project(pd-circuit)

# Define the path to the Pure Data sources (2):
set_pd_sources(${CMAKE_CURRENT_SOURCE_DIR}/Libraries)

# Set the output path for the externals (3):  
set_pd_external_path(${CMAKE_CURRENT_SOURCE_DIR}/binaries/)

# Compiling for Pd double precision (4)(optional)
set(PD_FLOATSIZE64 "OFF")

# Add one or several externals (5):   
add_pd_external(pd-circuit circuit~ ${CMAKE_CURRENT_SOURCE_DIR}/Source/circuit~.c)

file(GLOB KLU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/KLU/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/COLAMD/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/AMD/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/BTF/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/SuiteSparse_config/*.c")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Libraries/")

add_library(klu STATIC ${KLU_SOURCES})

add_library(circuit-simulator STATIC ${CMAKE_CURRENT_SOURCE_DIR}/Source/Simulator.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Source/Simulator.h ${CMAKE_CURRENT_SOURCE_DIR}/Source/Netlist.h ${CMAKE_CURRENT_SOURCE_DIR}/Source/MNA.h ${CMAKE_CURRENT_SOURCE_DIR}/Source/Components.h)
target_include_directories(circuit-simulator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries)
target_link_libraries(pd-circuit circuit-simulator klu)

