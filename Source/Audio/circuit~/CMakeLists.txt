cmake_minimum_required(VERSION 3.10)

project(pd-circuit)
add_pd_external(pd-circuit circuit~ ${CMAKE_CURRENT_SOURCE_DIR}/Source/circuit~.c)

file(GLOB KLU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/KLU/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/COLAMD/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/AMD/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/BTF/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/SuiteSparse_config/*.c")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Libraries/")

target_sources(pd-circuit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source/Simulator.cpp ${KLU_SOURCES})
target_link_libraries(pd-circuit PRIVATE else_shared)

if(WIN32)
    target_link_options(pd-circuit PUBLIC -static-libgcc -static-libstdc++ -static)
    target_link_libraries(pd-circuit PUBLIC "ws2_32;stdc++")
endif()

target_include_directories(pd-circuit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Libraries)
