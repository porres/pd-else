file(GLOB SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/libsamplerate/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/fftease/*.c)

add_library(else_shared SHARED ${SOURCES})

target_include_directories(else_shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared)

set_target_properties(else_shared PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                             ${PD_OUTPUT_PATH})
set_target_properties(else_shared PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                             ${PD_OUTPUT_PATH})
set_target_properties(else_shared PROPERTIES ARCHIVE_OUTPUT_DIRECTORY
                                             ${PD_OUTPUT_PATH})
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
  set_target_properties(
    else_shared PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG}
                           ${PD_OUTPUT_PATH})
  set_target_properties(
    else_shared PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}
                           ${PD_OUTPUT_PATH})
  set_target_properties(
    else_shared PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG}
                           ${PD_OUTPUT_PATH})
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

if(APPLE)
  target_link_options(else_shared PRIVATE -undefined dynamic_lookup)
elseif(WIN32)
  find_library(
    PD_LIBRARY
    NAMES pd
    HINTS ${PD_LIB_PATH})
  target_link_libraries(else_shared PRIVATE ws2_32 "${PDBINDIR}/pd.dll")
endif()

if(PD_FLOATSIZE64)
  target_compile_definitions(else_shared PRIVATE PD_FLOATSIZE=64)
endif()

function(message)
  if(NOT MESSAGE_QUIET)
    _message(${ARGN})
  endif()
endfunction()

message(STATUS "Configuring Opus")
set(MESSAGE_QUIET ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/opus EXCLUDE_FROM_ALL)
set(MESSAGE_QUIET OFF)
