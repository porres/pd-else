
if(NOT SMTG_ENABLE_VSTGUI_SUPPORT)
    return()
endif()

cmake_minimum_required(VERSION 3.15.0)

project(smtg-vst3-panner
    VERSION ${vstsdk_VERSION}.0
    DESCRIPTION "Steinberg VST 3 Panner example"
)

smtg_add_vst3plugin(panner 
    include/plugcontroller.h
    include/plugids.h
    include/plugprocessor.h
    include/version.h
    source/plugfactory.cpp
    source/plugcontroller.cpp
    source/plugprocessor.cpp
    resource/plug.uidesc
)

target_link_libraries(panner 
    PRIVATE
        vstgui_support
)

smtg_target_add_plugin_resources(panner
    RESOURCES
        resource/plug.uidesc
        resource/background.png
        resource/animation_knob.png
        resource/background_2x.png
        resource/background_3x.png
        resource/animation_knob_3x.png
)

smtg_target_add_plugin_snapshots(panner
    RESOURCES
        resource/A2EAF7DB320640F48EDE380DDF89562C_snapshot.png
        resource/A2EAF7DB320640F48EDE380DDF89562C_snapshot_2.0x.png
)

smtg_target_setup_as_vst3_example(panner)
