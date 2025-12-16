cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(pd-circuit)
endif()
pd_add_external(circuit~ ${CMAKE_CURRENT_SOURCE_DIR}/Source/circuit~.c)

file(GLOB KLU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/KLU/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/COLAMD/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/AMD/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/BTF/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/SuiteSparse_config/*.c")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Libraries/")

target_sources(circuit_tilde PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source/Simulator.cpp ${KLU_SOURCES})
target_link_libraries(circuit_tilde PRIVATE else_shared)

if(WIN32)
    target_link_options(circuit_tilde PUBLIC -static-libgcc -static-libstdc++ -static)
    target_link_libraries(circuit_tilde PUBLIC "ws2_32;stdc++")
endif()

target_include_directories(circuit_tilde PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries)
