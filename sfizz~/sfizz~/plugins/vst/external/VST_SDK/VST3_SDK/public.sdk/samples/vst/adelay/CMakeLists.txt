
cmake_minimum_required(VERSION 3.15.0)

project(smtg-vst3-adelay
    VERSION ${vstsdk_VERSION}.0
    DESCRIPTION "Steinberg VST 3 ADelay example"
)

smtg_add_vst3plugin(adelay     
    source/adelaycontroller.cpp
    source/adelaycontroller.h
    source/adelayids.h
    source/adelayprocessor.cpp
    source/adelayprocessor.h
    source/exampletest.cpp
    source/factory.cpp
    source/version.h
    ${SDK_ROOT}/public.sdk/source/vst/utility/test/ringbuffertest.cpp
    ${SDK_ROOT}/public.sdk/source/vst/utility/test/versionparsertest.cpp
)

smtg_target_setup_as_vst3_example(adelay)