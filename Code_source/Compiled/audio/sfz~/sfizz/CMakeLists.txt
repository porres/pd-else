if(WIN32)
    cmake_minimum_required(VERSION 3.15)
    cmake_policy(SET CMP0091 NEW)
else()
    cmake_minimum_required(VERSION 3.13)
endif()

# The project was splitted in 2 repositories:
# - `sfizz` for the library, which represents the main repository reference
# - `sfizz-ui` for UIs (plugins with a future standalone UI in mind).
# However, to keep the original structure, we assume this as "sfizz" project,
# containing the "libsfizz" sub project in the `library` git submodule.
project(sfizz
    DESCRIPTION  "SFZ sampler, providing AU, LV2, PureData and VST plugins."
    HOMEPAGE_URL "http://sfztools.github.io/sfizz"
    LANGUAGES    CXX C
    VERSION      1.2.2
)
set(PROJECT_REPOSITORY "https://github.com/sfztools/sfizz")
set(PROJECT_AUTHOR     "SFZTools")
set(PROJECT_EMAIL      "paul@ferrand.cc")

set(LV2_PLUGIN_VERSION_MINOR 10)
set(LV2_PLUGIN_VERSION_MICRO 3)

# External configuration CMake scripts
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};"
    "${CMAKE_CURRENT_SOURCE_DIR}/library/cmake;"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
)
include(BuildType)
include(OptionEx)

option_ex(PLUGIN_AU                "Enable AU plug-in build" APPLE)
option_ex(PLUGIN_LV2               "Enable LV2 plug-in build" ON)
option_ex(PLUGIN_LV2_UI            "Enable LV2 plug-in user interface" ON)
option_ex(PLUGIN_PUREDATA          "Enable Puredata plug-in build" OFF)
option_ex(PLUGIN_VST2              "Enable VST2 plug-in build (unsupported)" OFF)
option_ex(PLUGIN_VST3              "Enable VST3 plug-in build" ON)
option_ex(SFIZZ_USE_SYSTEM_LV2     "Use LV2 headers preinstalled on system" OFF)
option_ex(SFIZZ_USE_SYSTEM_VST3SDK "Use VST3SDK source files preinstalled on system" OFF)

set(MIDI_CC_COUNT 512 CACHE STRING "Maximum amount of Control Change Messages")

if(PLUGIN_LV2 AND SFIZZ_SNDFILE_STATIC)
    set(LV2_PLUGIN_SPDX_LICENSE_ID "LGPL-3.0-only")
endif()

# Set Windows compatibility level to 10 required by VSTGUI
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

# Override sfizz CXX standard since vstgui requires 17 anyway
if (PLUGIN_AU OR PLUGIN_LV2_UI OR PLUGIN_VST3 OR PLUGIN_VST2)
    set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to be used")
endif()

include(SfizzConfig) # Re-used for this project
include(BundleDylibs)

add_subdirectory(library)
add_subdirectory(plugins)

# Windows installer
if(WIN32)
    include(VSTConfig)
    configure_file(${PROJECT_SOURCE_DIR}/scripts/innosetup.iss.in
        ${PROJECT_BINARY_DIR}/innosetup.iss @ONLY)
endif()

# Leave this at the end so that the vst/lv2 directories are registered
if(NOT MSVC)
    include(PluginsUninstall)
endif()

show_build_info_if_needed()
