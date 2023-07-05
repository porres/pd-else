
cmake_minimum_required(VERSION 3.15.0)

if(NOT SMTG_AAX_SDK_PATH OR NOT SMTG_ADD_VSTGUI)
    return()
endif()

project(smtg-vst3-hostchecker_aax
    VERSION ${vstsdk_VERSION}.0
    DESCRIPTION "Steinberg VST 3 hostchecker AAX example"
)

include(SMTG_AddAAXLibrary)

smtg_add_aaxplugin(hostchecker_aax
    ../hostchecker/resource/hostchecker.uidesc
    ../hostchecker/source/cids.h
    ../hostchecker/source/editorsizecontroller.cpp
    ../hostchecker/source/editorsizecontroller.h
    ../hostchecker/source/eventlistcheck.cpp
    ../hostchecker/source/eventlistcheck.h
    ../hostchecker/source/eventlogdatabrowsersource.cpp
    ../hostchecker/source/eventlogdatabrowsersource.h
    ../hostchecker/source/eventlogger.cpp
    ../hostchecker/source/eventlogger.h
    ../hostchecker/source/factory.cpp
    ../hostchecker/source/hostcheck.cpp
    ../hostchecker/source/hostcheck.h
    ../hostchecker/source/hostcheckercontroller.cpp
    ../hostchecker/source/hostcheckercontroller.h
    ../hostchecker/source/hostcheckerprocessor.cpp
    ../hostchecker/source/hostcheckerprocessor.h
    ../hostchecker/source/logevents.h
    ../hostchecker/source/parameterchangescheck.cpp
    ../hostchecker/source/parameterchangescheck.h
    ../hostchecker/source/processcontextcheck.cpp
    ../hostchecker/source/processcontextcheck.h
    ../hostchecker/source/processsetupcheck.cpp
    ../hostchecker/source/processsetupcheck.h
    ../hostchecker/source/version.h
    source/hostchecker_aax.cpp
)

target_include_directories(hostchecker_aax
    PUBLIC 
        ${SDK_ROOT}/public.sdk/samples/vst/hostchecker/source
)

target_link_libraries(hostchecker_aax
    PRIVATE
        vstgui_support
        aaxwrapper
)

smtg_target_add_plugin_resources(hostchecker_aax
    RESOURCES
        ../hostchecker/resource/hostchecker.uidesc
        ../hostchecker/resource/bypass.png
        ../hostchecker/resource/knob.png
)

smtg_target_setup_as_vst3_example(hostchecker_aax)
