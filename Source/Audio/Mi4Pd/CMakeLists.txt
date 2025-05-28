cmake_minimum_required(VERSION 3.5)
project(Mi4Pd)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    if (NOT APPLE)
        message(STATUS "Target is ARM or Apple")
    else()
        add_definitions(-DTEST)
    endif()
else()
    message(STATUS "Not ARM build is experimental")
    add_definitions(-DTEST)
endif()

add_subdirectory(mi)
set_target_properties(MILib PROPERTIES POSITION_INDEPENDENT_CODE ON)
include_directories(mi)


# add_pd_external(grds~ grds~ "${CMAKE_CURRENT_SOURCE_DIR}/grds/grids.cpp" LINK_LIBRARIES MILib)
# target_compile_definitions(grds~ PRIVATE _USE_MATH_DEFINES) # if you enable it later

add_pd_external(brds_tilde birds~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/birds~.cpp")
target_link_libraries(brds_tilde PRIVATE MILib)

add_pd_external(clds_tilde clouds~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/clouds~.cpp")
target_link_libraries(clds_tilde PRIVATE MILib)

add_pd_external(clds_reverb_tilde clouds.reverb~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/clouds.reverb~.cpp")
target_link_libraries(clds_reverb_tilde PRIVATE MILib)

add_pd_external(lmnts_tilde elements~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/elements~.cpp")
target_link_libraries(lmnts_tilde PRIVATE MILib)

add_pd_external(rngs_tilde rings~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/rings~.cpp")
target_link_libraries(rngs_tilde PRIVATE MILib)

add_pd_external(rngs_chorus_tilde rings.chorus~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/rings.chorus~.cpp")
target_link_libraries(rngs_chorus_tilde PRIVATE MILib)

add_pd_external(rngs_ensemble_tilde rings.ensemble~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/rings.ensemble~.cpp")
target_link_libraries(rngs_ensemble_tilde PRIVATE MILib)

add_pd_external(rngs_reverb_tilde rings.reverb~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/rings.reverb~.cpp")
target_link_libraries(rngs_reverb_tilde PRIVATE MILib)

add_pd_external(wrps_tilde warps~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/warps~.cpp")
target_link_libraries(wrps_tilde PRIVATE MILib)

add_compile_definitions(_USE_MATH_DEFINES)
