
cmake_minimum_required(VERSION 3.15.0)

project(smtg-vst3-multiple-program-changes
    VERSION ${vstsdk_VERSION}.0
    DESCRIPTION "Steinberg VST 3 Multiple Program Changes example"
)
smtg_add_vst3plugin(multiple-program-changes
    source/plug.cpp
    source/plug.h
    source/plugcids.h
    source/plugcontroller.cpp
    source/plugcontroller.h
    source/plugentry.cpp
    source/plugparamids.h
    source/version.h
)

smtg_target_setup_as_vst3_example(multiple-program-changes)
