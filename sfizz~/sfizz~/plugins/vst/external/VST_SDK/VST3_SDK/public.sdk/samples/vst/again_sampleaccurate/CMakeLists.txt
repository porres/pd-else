
cmake_minimum_required(VERSION 3.15.0)

project(smtg-vst3-again-sampleaccurate
    VERSION ${vstsdk_VERSION}.0
    DESCRIPTION "Steinberg VST 3 AGain example"
)

smtg_add_vst3plugin(again-sampleaccurate 
    source/agsa_controller.cpp
    source/agsa_factory.cpp
    source/agsa_processor.cpp
    source/agsa.h
    source/tutorial.cpp
    source/tutorial.h
    source/version.h
    ${SDK_ROOT}/public.sdk/source/vst/utility/test/sampleaccuratetest.cpp
    ${SDK_ROOT}/public.sdk/source/vst/utility/test/rttransfertest.cpp
)

target_link_libraries(again-sampleaccurate
    PRIVATE
        sdk_hosting
)

smtg_target_setup_as_vst3_example(again-sampleaccurate)