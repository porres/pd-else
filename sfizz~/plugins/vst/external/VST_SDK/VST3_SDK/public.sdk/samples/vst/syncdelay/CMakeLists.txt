
cmake_minimum_required(VERSION 3.15.0)

project(smtg-vst3-syncdelay
    VERSION ${vstsdk_VERSION}.0
    DESCRIPTION "Steinberg VST 3 Sync Delay example"
)

smtg_add_vst3plugin(syncdelay     
    source/sync.h
    source/syncdelaycontroller.cpp
    source/syncdelaycontroller.h
    source/syncdelayfactory.cpp
    source/syncdelayids.h
    source/syncdelayprocessor.cpp
    source/syncdelayprocessor.h
    source/syncdelayversion.h
)

smtg_target_setup_as_vst3_example(syncdelay)
