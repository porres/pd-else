cmake_minimum_required(VERSION 3.15)

# Add Linux-specific configurations
if(UNIX
   AND NOT APPLE
   AND NOT EMSCRIPTEN)
  # Set proper flags for forward compatibility
  add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FORTIFY_SOURCE=2 -O2")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORTIFY_SOURCE=2 -O2")

  # OpenSSL 3.x configuration
  set(OPENSSL_USE_STATIC_LIBS TRUE)
  find_package(OpenSSL 3.0.0 REQUIRED)
  if(NOT OPENSSL_FOUND)
    message(FATAL_ERROR "OpenSSL 3.0.0 or higher is required")
  endif()
  message(STATUS "Found OpenSSL ${OPENSSL_VERSION}")
  add_definitions(-DHAVE_OPENSSL_3_0)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_MACOSX_RPATH ON)

# pd.cmake
cmake_policy(SET CMP0135 NEW)
set(PDCMAKE_FILE ${CMAKE_BINARY_DIR}/pd.cmake)
if(NOT EXISTS ${PDCMAKE_FILE})
  message(STATUS "Downloading pd.cmake")
  file(
    DOWNLOAD
    https://raw.githubusercontent.com/pure-data/pd.cmake/refs/tags/v0.2.8/pd.cmake
    ${PDCMAKE_FILE}
    SHOW_PROGRESS
    STATUS DOWNLOAD_STATUS)
endif()
set(PD_OUTPUT_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/else"
    CACHE STRING "Externals binary path")
include(${PDCMAKE_FILE})

# Add static linking on Windows
if(WIN32)
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

option(ENABLE_OPUS "Enable opus for pdlink~ stream compression" ON)
if(!ENABLE_OPUS)
  add_compile_definitions(ENABLE_OPUS=0)
endif()

if(UNIX AND NOT APPLE)
  set(CMAKE_BUILD_RPATH "$ORIGIN")
  set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()
if(APPLE)
  set(CMAKE_BUILD_RPATH "@loader_path")
  set(CMAKE_INSTALL_RPATH "@loader_path")
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET
    "10.9"
    CACHE STRING "Minimum OS X deployment version")
set(CMAKE_OSX_ARCHITECTURES
    "arm64;x86_64"
    CACHE STRING "" FORCE)

project(ELSE LANGUAGES C CXX)

# include(${CMAKE_CURRENT_SOURCE_DIR}/pd.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/shared.cmake)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      "Release"
      CACHE STRING "Default build type: Release" FORCE)
endif()

file(GLOB else_sources ${CMAKE_CURRENT_SOURCE_DIR}/Source/Control/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/Source/Audio/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/Source/Extra/Aliases/*.c)

file(GLOB aubio_sources
     ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/aubio/src/**/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/aubio/src/*.c)

include_directories(
  ${PD_SOURCES_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared
  ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/aubio/src)

foreach(file ${else_sources})
  get_filename_component(name ${file} NAME)
  string(REGEX REPLACE "\\.[^.]*$" "" name_we ${name})
  string(REGEX REPLACE "~" "_tilde" target_name ${name_we})
  pd_add_external(${name_we} ${file})
  target_link_libraries(${target_name} PRIVATE else_shared)

  # If target is streamout_tilde, add static libogg and libvorbis linking on
  # macOS
  if(${target_name} STREQUAL "streamout_tilde")
    if(APPLE)
      set(OGG_STATIC_LIB "/opt/homebrew/lib/libogg.a")
      set(VORBIS_STATIC_LIB "/opt/homebrew/lib/libvorbis.a")
      set(VORBISENC_STATIC_LIB "/opt/homebrew/lib/libvorbisenc.a")

      set(OGG_INCLUDE_DIR "/opt/homebrew/include")
      set(VORBIS_INCLUDE_DIR "/opt/homebrew/include")
      set(VORBISENC_INCLUDE_DIR "/opt/homebrew/include")

      if(EXISTS ${OGG_STATIC_LIB}
         AND EXISTS ${VORBIS_STATIC_LIB}
         AND EXISTS ${VORBISENC_STATIC_LIB})
        message(STATUS "Found static libogg, libvorbis, and libvorbisenc")
        target_include_directories(
          ${target_name} PRIVATE ${OGG_INCLUDE_DIR} ${VORBIS_INCLUDE_DIR}
                                 ${VORBISENC_INCLUDE_DIR})
        target_link_libraries(
          ${target_name} PRIVATE ${OGG_STATIC_LIB} ${VORBIS_STATIC_LIB}
                                 ${VORBISENC_STATIC_LIB} m)
      else()
        message(
          FATAL_ERROR
            "Static libogg, libvorbis, or libvorbisenc not found in /opt/homebrew/lib"
        )
      endif()
    elseif(EMSCRIPTEN)
      message(WARNING "libogg not support by now")
    else()
      # Keep original find_library logic for Linux and other Unix
      find_path(OGG_INCLUDE_DIR ogg/ogg.h PATHS /opt/homebrew/include
                                                /usr/local/include)
      find_library(
        OGG_LIBRARY
        NAMES ogg
        PATHS /opt/homebrew/lib /usr/local/lib)

      find_path(VORBIS_INCLUDE_DIR vorbis/codec.h PATHS /opt/homebrew/include
                                                        /usr/local/include)
      find_library(
        VORBIS_LIBRARY
        NAMES vorbis
        PATHS /opt/homebrew/lib /usr/local/lib)

      find_path(VORBISENC_INCLUDE_DIR vorbis/vorbisenc.h
                PATHS /opt/homebrew/include /usr/local/include)
      find_library(
        VORBISENC_LIBRARY
        NAMES vorbisenc
        PATHS /opt/homebrew/lib /usr/local/lib)

      if(OGG_INCLUDE_DIR
         AND OGG_LIBRARY
         AND VORBIS_INCLUDE_DIR
         AND VORBIS_LIBRARY
         AND VORBISENC_INCLUDE_DIR
         AND VORBISENC_LIBRARY)
        message(STATUS "Found libogg, libvorbis, and libvorbisenc")
        target_include_directories(
          ${target_name} PRIVATE ${OGG_INCLUDE_DIR} ${VORBIS_INCLUDE_DIR}
                                 ${VORBISENC_INCLUDE_DIR})
        target_link_libraries(
          ${target_name} PRIVATE ${OGG_LIBRARY} ${VORBIS_LIBRARY}
                                 ${VORBISENC_LIBRARY} m)
      else()
        message(FATAL_ERROR "libogg, libvorbis, or libvorbisenc not found")
      endif()
    endif()
  endif()
  # and of if
endforeach()

target_sources(beat_tilde PRIVATE ${aubio_sources})

add_subdirectory(Source/Audio/circuit~)
add_subdirectory(Source/Audio/plaits~)
add_subdirectory(Source/Audio/sfont~)
add_subdirectory(Source/Audio/sfz~)
add_subdirectory(Source/Shared/link)
add_subdirectory(Source/Shared/ffmpeg)

# ╭──────────────────────────────────────╮
# │            pdlua library             │
# ╰──────────────────────────────────────╯
include(FetchContent)
FetchContent_Declare(
  pd_lua
  GIT_REPOSITORY https://github.com/EL-LOCUS-SOLUS/pd-lua.git
  GIT_TAG master)
FetchContent_MakeAvailable(pd_lua)
target_link_libraries(else PUBLIC pdlua)

# ╭──────────────────────────────────────╮
# │          Ambisonics library          │
# ╰──────────────────────────────────────╯
if (ELSE_WITH_SAF)
    # set(FETCHCONTENT_SOURCE_DIR_PDSAF "${CMAKE_CURRENT_SOURCE_DIR}/../pd-saf")
    FetchContent_Declare(
      pdsaf
      GIT_REPOSITORY https://github.com/EL-LOCUS-SOLUS/pd-saf.git
      GIT_TAG main)
    FetchContent_MakeAvailable(pdsaf)
    file(GLOB pdsaf_lua ${pdsaf_SOURCE_DIR}/Sources/*.pd_lua)
    file(COPY ${pdsaf_lua} DESTINATION ${PD_OUTPUT_PATH})
endif()

#╭──────────────────────────────────────╮
#│             Target links             │
#╰──────────────────────────────────────╯
target_link_libraries(play.file_tilde PRIVATE ffmpeg)
target_link_libraries(streamin_tilde PRIVATE ffmpeg)
target_link_libraries(streamout_tilde PRIVATE ffmpeg)
target_link_libraries(sfload PRIVATE ffmpeg)
target_link_libraries(sfinfo PRIVATE ffmpeg)

# Add OpenSSL linking for Linux builds
if(UNIX
   AND NOT APPLE
   AND NOT EMSCRIPTEN)
  foreach(target streamin_tilde streamout_tilde sfload play.file_tilde sfinfo)
    target_link_libraries(
      ${target} PRIVATE "-Wl,--whole-archive" OpenSSL::SSL OpenSSL::Crypto
                        "-Wl,--no-whole-archive" pthread dl)
    target_compile_definitions(
      ${target} PRIVATE OPENSSL_NO_KRB5 OPENSSL_NO_ENGINE OPENSSL_API_3_0)
  endforeach()
endif()

target_link_libraries(pdlink PUBLIC link)
target_link_libraries(pdlink_tilde PUBLIC link opus)
target_include_directories(
  pdlink_tilde PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/libsamplerate
                      ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/opus)

set(pdf_help "${CMAKE_CURRENT_SOURCE_DIR}/Documentation/README.pdf")
file(GLOB help_files "${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Help-files/*")
file(GLOB extra_files ${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Extra-files/*
     ${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Extra-files/audio/*)

file(GLOB merda_files
     "${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Merda/Modules/*")
file(GLOB tcl_files ${CMAKE_CURRENT_SOURCE_DIR}/Source/Extra/*.tcl
     ${CMAKE_CURRENT_SOURCE_DIR}/Source/Extra/tkdnd)
file(GLOB pdlua_files ${CMAKE_CURRENT_SOURCE_DIR}/Source/Audio/*.pd_lua
     ${CMAKE_CURRENT_SOURCE_DIR}/Source/Control/*.pd_lua)
file(GLOB abstractions ${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Audio/*.pd
     ${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Control/*.pd
     ${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Extra/*)
file(COPY ${pdf_help} ${extra_files} ${help_files} ${abstractions} ${tcl_files}
          ${pdlua_files} DESTINATION ${PD_OUTPUT_PATH})
file(COPY ${merda_files} DESTINATION ${PD_OUTPUT_PATH})
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Source/Audio/sfz~/sfz"
     DESTINATION ${PD_OUTPUT_PATH})
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Documentation/Live-Electronics-Tutorial"
     DESTINATION ${PD_OUTPUT_PATH})
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
     DESTINATION ${PD_OUTPUT_PATH}/LICENSE)
file(COPY "${pd_lua_SOURCE_DIR}/pd.lua" DESTINATION ${PD_OUTPUT_PATH}/)
file(REMOVE_RECURSE ${PD_OUTPUT_PATH}/audio)

# Log the object count
function(count_files_and_directories dir extension file_count count_dirs)
  file(GLOB files "${dir}/*")
  foreach(file ${files})
    get_filename_component(file_name "${file}" NAME)
    if(NOT file_name MATCHES "^\\."
       AND NOT file_name MATCHES "-help\\.pd$"
       AND NOT file_name MATCHES ".unit\\.pd$"
       AND NOT file_name MATCHES "about.MERDA.pd"
       AND NOT (IS_DIRECTORY ${file} AND NOT count_dirs)) # Exclude hidden files
                                                          # and help files
      math(EXPR ${file_count} "${${file_count}} + 1")
    endif()
  endforeach()
  set(${file_count}
      ${${file_count}}
      PARENT_SCOPE)
endfunction()

count_files_and_directories("${CMAKE_CURRENT_SOURCE_DIR}/Source/Control" "c"
                            control_c_file_count YES)
count_files_and_directories("${CMAKE_CURRENT_SOURCE_DIR}/Source/Audio" "c"
                            signal_c_file_count YES)
count_files_and_directories("${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Control"
                            "pd" control_pd_file_count YES)
count_files_and_directories("${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Audio"
                            "pd" signal_pd_file_count YES)
count_files_and_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/Abstractions/Merda/Modules" "pd"
  modules_pd_file_count NO)
math(EXPR total_compiled "${control_c_file_count} + ${signal_c_file_count}")
math(EXPR total_abstractions
     "${control_pd_file_count} + ${signal_pd_file_count}")

message(
  STATUS
    "${total_compiled} coded objects (${signal_c_file_count} signal objects / ${control_c_file_count} control objects)"
)
message(
  STATUS
    "${total_abstractions} abstractions (${signal_pd_file_count} signal objects / ${control_pd_file_count} control objects)"
)
message(STATUS "Merda modules: ${modules_pd_file_count}")
