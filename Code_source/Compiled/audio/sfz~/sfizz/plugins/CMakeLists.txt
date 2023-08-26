add_library(plugins-common STATIC EXCLUDE_FROM_ALL
    "common/plugin/RMSFollower.h"
    "common/plugin/MessageUtils.h"
    "common/plugin/MessageUtils.cpp"
    "common/plugin/InstrumentDescription.h"
    "common/plugin/InstrumentDescription.cpp"
    "common/plugin/NativeHelpers.h"
    "common/plugin/NativeHelpers.cpp"
    "common/plugin/SfizzFileScan.h"
    "common/plugin/SfizzFileScan.cpp"
    "common/plugin/FileTrie.h"
    "common/plugin/FileTrie.cpp"
    "common/plugin/SfizzForeignPaths.h"
    "common/plugin/SfizzForeignPaths.cpp"
    "common/plugin/SfizzSettings.h"
    "common/plugin/SfizzSettings.cpp")
if(APPLE)
    target_sources(plugins-common PRIVATE
        "common/plugin/SfizzForeignPaths.mm"
        "common/plugin/SfizzSettings.mm"
        "common/plugin/NativeHelpers.mm")
    set_property(
        SOURCE "common/plugin/SfizzForeignPaths.mm"
               "common/plugin/SfizzSettings.mm"
               "common/plugin/NativeHelpers.mm"
        APPEND_STRING
        PROPERTY COMPILE_FLAGS " -fobjc-arc")
endif()
target_include_directories(plugins-common PUBLIC "common")
target_link_libraries(plugins-common
    PUBLIC sfizz::spin_mutex
    PUBLIC sfizz::simde sfizz::filesystem absl::strings
    PRIVATE sfizz::pugixml
    PRIVATE sfizz::internal sfizz::sfizz)
add_library(sfizz::plugins-common ALIAS plugins-common)

# Link system dependencies
if(WIN32)
elseif(ANDROID)
elseif(APPLE)
    target_link_libraries(plugins-common PRIVATE
        "${APPLE_FOUNDATION_LIBRARY}")
else()
endif()

if((PLUGIN_LV2 AND PLUGIN_LV2_UI) OR PLUGIN_VST3 OR PLUGIN_AU OR PLUGIN_VST2)
    add_subdirectory(editor)
endif()

if(PLUGIN_LV2)
    add_subdirectory(lv2)
endif()

if((PLUGIN_VST3 OR PLUGIN_AU OR PLUGIN_VST2) AND NOT (PROJECT_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc).*"))
    add_subdirectory(vst)
endif()

if(PLUGIN_PUREDATA)
    add_subdirectory(puredata)
endif()
