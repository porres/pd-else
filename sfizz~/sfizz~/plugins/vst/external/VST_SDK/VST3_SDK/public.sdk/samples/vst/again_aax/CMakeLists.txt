
cmake_minimum_required(VERSION 3.15.0)

project(smtg-vst3-again
    VERSION ${vstsdk_VERSION}.0
    DESCRIPTION "Steinberg VST 3 AGain AAX example"
)

if(NOT SMTG_AAX_SDK_PATH OR NOT SMTG_ADD_VSTGUI)
    return()
endif()

include(SMTG_AddAAXLibrary)

set(again_sources
    source/againaax.cpp
    ../again/source/again.cpp
    ../again/source/again.h
    ../again/source/againcids.h
    ../again/source/againcontroller.cpp
    ../again/source/againcontroller.h
    ../again/source/againentry.cpp
    ../again/source/againparamids.h
    ../again/source/againprocess.h
    ../again/source/againsidechain.cpp
    ../again/source/againsidechain.h
    ../again/source/againuimessagecontroller.h
    ../again/source/version.h
    ../again/resource/again.uidesc
)

set(target again_aax)
smtg_add_aaxplugin(${target} ${again_sources})
smtg_target_configure_version_file(${target})
set_target_properties(${target}
    PROPERTIES
        ${SDK_IDE_PLUGIN_EXAMPLES_FOLDER}
)
target_include_directories(${target}
    PUBLIC
        ${SDK_ROOT}/public.sdk/samples/vst/again/source
)

target_compile_features(${target}
    PUBLIC
        cxx_std_17
)

target_link_libraries(${target}
    PRIVATE
        sdk
        vstgui_support
        aaxwrapper
)
smtg_target_add_plugin_resources(${target}
    RESOURCES
        ../again/resource/again.uidesc
        ../again/resource/background.png
        ../again/resource/slider_background.png
        ../again/resource/slider_handle.png
        ../again/resource/slider_handle_2.0x.png
        ../again/resource/vu_on.png
        ../again/resource/vu_off.png
)

if(SMTG_MAC)
    smtg_target_set_bundle(${target}
        BUNDLE_IDENTIFIER "com.steinberg.vst3.${target}"
        INFOPLIST "${CMAKE_CURRENT_LIST_DIR}/../again/mac/Info.plist" PREPROCESS
    )
elseif(SMTG_WIN)
    target_sources(${target} 
        PRIVATE 
            ../again/resource/again.rc
    )
    # remove warnings from VST2
    if(NOT MINGW)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    endif(NOT MINGW)
endif(SMTG_MAC)    
