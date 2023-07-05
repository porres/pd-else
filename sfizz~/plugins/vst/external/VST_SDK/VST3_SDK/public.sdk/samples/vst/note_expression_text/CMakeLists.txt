
if(NOT SMTG_ENABLE_VSTGUI_SUPPORT)
    return()
endif()

cmake_minimum_required(VERSION 3.15.0)

project(smtg-vst3-noteexpressiontext
    VERSION ${vstsdk_VERSION}.0
    DESCRIPTION "Steinberg VST 3 Note Expression Text example"
)

smtg_add_vst3plugin(noteexpressiontext 
   source/plug.cpp
    source/plug.h
    source/plugcids.h
    source/plugcontroller.cpp
    source/plugcontroller.h
    source/plugentry.cpp
    source/plugparamids.h
    source/version.h
    resource/plug.uidesc
)

target_link_libraries(noteexpressiontext
    PRIVATE
        vstgui_support
)

smtg_target_add_plugin_resources(noteexpressiontext
    RESOURCES
        resource/plug.uidesc
)

smtg_target_setup_as_vst3_example(noteexpressiontext)
