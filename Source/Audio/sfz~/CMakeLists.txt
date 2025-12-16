cmake_minimum_required(VERSION 3.10)

project(sfizz
    DESCRIPTION  "SFZ sampler, providing AU, LV2, PureData and VST plugins."
    HOMEPAGE_URL "http://sfztools.github.io/sfizz"
    LANGUAGES    CXX C
)

set(BASEFLAGS
    "-DENABLE_LTO=OFF"
    "-DSFIZZ_JACK=OFF"
    "-DSFIZZ_RENDER=OFF"
    "-DSFIZZ_SHARED=OFF"
    "-DPLUGIN_AU=OFF"
    "-DPLUGIN_LV2=OFF"
    "-DPLUGIN_LV2_UI=OFF"
    "-DPLUGIN_PUREDATA=OFF"
    "-DPLUGIN_VST2=OFF"
    "-DPLUGIN_VST3=OFF"
    "-DSFIZZ_BENCHMARKS=OFF"
    "-DSFIZZ_DEMOS=OFF"
    "-DSFIZZ_DEVTOOLS=OFF"
    "-DSFIZZ_TESTS=OFF"
    "-DSFIZZ_USE_SNDFILE=OFF"
    "-DSFIZZ_SNDFILE_STATIC=OFF"
    "-DSFIZZ_USE_SYSTEM_ABSEIL=OFF"
    "-DSFIZZ_USE_SYSTEM_CATCH=OFF"
    "-DSFIZZ_USE_SYSTEM_CXXOPTS=OFF"
    "-DSFIZZ_USE_SYSTEM_GHC_FS=OFF"
    "-DSFIZZ_USE_SYSTEM_LV2=OFF"
    "-DSFIZZ_USE_SYSTEM_SIMDE=OFF"
    "-DSFIZZ_USE_SYSTEM_KISS_FFT=OFF"
    "-DSFIZZ_USE_SYSTEM_PUGIXML=OFF"
    "-DSFIZZ_USE_SYSTEM_VST3SDK=OFF"
    "-DSFIZZ_PROFILE_BUILD=OFF"
    "-DSFIZZ_RELEASE_ASSERTS=OFF"
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}"
    "-DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}"
    # Add ARM-specific flags
    "-DSFIZZ_USE_X86_OPTIMIZATIONS=OFF"
    "-DSFIZZ_DISABLE_X86_OPTIMIZATIONS=ON"
    "-DSFIZZ_SSE_LEVEL=0"
    "-DSFIZZ_USE_ARM_OPTIMIZATIONS=ON"
    "-DSFIZZ_USE_NEON=ON"
    "-DSFIZZ_VST_ARCH=arm64"
    # Pass through the toolchain file
    "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
)

message(STATUS "Configuring sfizz")
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/sfizz/library/build")
execute_process(COMMAND ${CMAKE_COMMAND} .. -G${CMAKE_GENERATOR} ${BASEFLAGS} ${MSVC_TARGET_ARCH} OUTPUT_QUIET ERROR_QUIET
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sfizz/library/build
)


set(SFIZZ_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sfizz/library/build/library/lib)

set(SFIZZ_LIBS
    ${SFIZZ_LIB_DIR}/libsfizz.a
    ${SFIZZ_LIB_DIR}/libsfizz_internal.a
    ${SFIZZ_LIB_DIR}/libsfizz_cephes.a
    ${SFIZZ_LIB_DIR}/libsfizz_filesystem_impl.a
    ${SFIZZ_LIB_DIR}/libsfizz_fmidi.a
    ${SFIZZ_LIB_DIR}/libsfizz_hiir_polyphase_iir2designer.a
    ${SFIZZ_LIB_DIR}/libsfizz_import.a
    ${SFIZZ_LIB_DIR}/libsfizz_kissfft.a
    ${SFIZZ_LIB_DIR}/libsfizz_messaging.a
    ${SFIZZ_LIB_DIR}/libsfizz_parser.a
    ${SFIZZ_LIB_DIR}/libsfizz_pugixml.a
    ${SFIZZ_LIB_DIR}/libsfizz_spin_mutex.a
    ${SFIZZ_LIB_DIR}/libsfizz_spline.a
    ${SFIZZ_LIB_DIR}/libsfizz_tunings.a
    ${SFIZZ_LIB_DIR}/libsfizz_cpuid.a
    ${SFIZZ_LIB_DIR}/libabsl_hash.a
    ${SFIZZ_LIB_DIR}/libabsl_strings.a
    ${SFIZZ_LIB_DIR}/libabsl_bad_optional_access.a
    ${SFIZZ_LIB_DIR}/libabsl_bad_variant_access.a
    ${SFIZZ_LIB_DIR}/libabsl_base.a
    ${SFIZZ_LIB_DIR}/libabsl_city.a
    ${SFIZZ_LIB_DIR}/libabsl_civil_time.a
    ${SFIZZ_LIB_DIR}/libabsl_cord.a
    ${SFIZZ_LIB_DIR}/libabsl_cord_internal.a
    ${SFIZZ_LIB_DIR}/libabsl_cordz_functions.a
    ${SFIZZ_LIB_DIR}/libabsl_cordz_handle.a
    ${SFIZZ_LIB_DIR}/libabsl_cordz_info.a
    ${SFIZZ_LIB_DIR}/libabsl_crc32c.a
    ${SFIZZ_LIB_DIR}/libabsl_crc_cord_state.a
    ${SFIZZ_LIB_DIR}/libabsl_crc_cpu_detect.a
    ${SFIZZ_LIB_DIR}/libabsl_crc_internal.a
    ${SFIZZ_LIB_DIR}/libabsl_debugging_internal.a
    ${SFIZZ_LIB_DIR}/libabsl_demangle_internal.a
    ${SFIZZ_LIB_DIR}/libabsl_exponential_biased.a
    ${SFIZZ_LIB_DIR}/libabsl_graphcycles_internal.a
    ${SFIZZ_LIB_DIR}/libabsl_hashtablez_sampler.a
    ${SFIZZ_LIB_DIR}/libabsl_int128.a
    ${SFIZZ_LIB_DIR}/libabsl_log_severity.a
    ${SFIZZ_LIB_DIR}/libabsl_low_level_hash.a
    ${SFIZZ_LIB_DIR}/libabsl_malloc_internal.a
    ${SFIZZ_LIB_DIR}/libabsl_raw_hash_set.a
    ${SFIZZ_LIB_DIR}/libabsl_raw_logging_internal.a
    ${SFIZZ_LIB_DIR}/libabsl_spinlock_wait.a
    ${SFIZZ_LIB_DIR}/libabsl_stacktrace.a
    ${SFIZZ_LIB_DIR}/libabsl_strings_internal.a
    ${SFIZZ_LIB_DIR}/libabsl_symbolize.a
    ${SFIZZ_LIB_DIR}/libabsl_synchronization.a
    ${SFIZZ_LIB_DIR}/libabsl_throw_delegate.a
    ${SFIZZ_LIB_DIR}/libabsl_time.a
    ${SFIZZ_LIB_DIR}/libabsl_time_zone.a
    ${SFIZZ_LIB_DIR}/libst_audiofile.a
    ${SFIZZ_LIB_DIR}/libst_audiofile_formats.a
    ${SFIZZ_LIB_DIR}/libaiff.a
)

add_custom_target(
    sfizz
    ALL
    DEPENDS ${SFIZZ_LIBS}
)

add_custom_command(
    OUTPUT ${SFIZZ_LIBS}
    COMMAND ${CMAKE_COMMAND} --build .
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sfizz/library/build
    VERBATIM
)

add_custom_target(sfizz_build
    DEPENDS ${SFIZZ_LIBS}
)

pd_add_external(sfz~ sfz~.c)
target_link_libraries(sfz_tilde PRIVATE ${SFIZZ_LIBS} else_shared stdc++)
target_include_directories(sfz_tilde PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sfizz/library/src)
target_compile_definitions(sfz_tilde PRIVATE MIDI_CC_COUNT=127 SFIZZ_VERSION="1.2.2")
add_dependencies(sfz_tilde sfizz)

if(UNIX AND NOT APPLE)
    target_link_libraries(sfz_tilde PUBLIC m)
endif()
if(WIN32)
    target_link_options(sfz_tilde PUBLIC -static-libgcc -static-libstdc++ -static)
    target_link_libraries(sfz_tilde PUBLIC "ws2_32;iphlpapi;")
endif()
