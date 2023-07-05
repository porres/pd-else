
if(NOT SMTG_ENABLE_VSTGUI_SUPPORT)
    return()
endif()

cmake_minimum_required(VERSION 3.15.0)

project(smtg-vst3-pitchnames
    VERSION ${vstsdk_VERSION}.0
    DESCRIPTION "Steinberg VST 3 Pitch Names example"
)

smtg_add_vst3plugin(pitchnames 
    source/factory.cpp
    source/pitchnames.cpp
    source/pitchnames.h
    source/pitchnamesdatabrowsersource.cpp
    source/pitchnamesdatabrowsersource.h
    source/version.h
)

target_link_libraries(pitchnames
    PRIVATE
        vstgui_support
)

smtg_target_setup_as_vst3_example(pitchnames)
