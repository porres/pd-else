
cmake_minimum_required(VERSION 3.4.3)

project(smtg_public_sdk VERSION 1.0.0)

#--sdk common----------------
if(VST_SDK)
    set(VST_COMMON_SRC
        source/vst/vstpresetfile.cpp
        source/vst/vstpresetfile.h
    )
endif(VST_SDK)
add_library(sdk_common
    STATIC
        source/common/commoniids.cpp
        source/common/openurl.cpp
        source/common/openurl.h
        source/common/readfile.cpp
        source/common/readfile.h
        source/common/systemclipboard.h
        source/common/systemclipboard_linux.cpp
        source/common/systemclipboard_win32.cpp
        source/common/threadchecker.h
        source/common/threadchecker_linux.cpp
        source/common/threadchecker_win32.cpp
        ${VST_COMMON_SRC}
)
if(SMTG_MAC)
    target_sources(sdk_common 
        PRIVATE
            source/common/threadchecker_mac.mm
            source/common/systemclipboard_mac.mm
    )
endif(SMTG_MAC)

if(NOT VST_SDK)
    target_sources(sdk_common 
        PRIVATE
            source/common/pvaluecontainer.cpp
            source/common/pvaluecontainer.h
            source/common/pattributes.cpp
            source/common/pattributes.h
    )
endif(NOT VST_SDK)

# add dependencies
target_link_libraries(sdk_common 
    PUBLIC 
        base
        pluginterfaces
)
target_include_directories(sdk_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
smtg_target_setup_universal_binary(sdk_common)

#--sdk plugin----------------
add_library(sdk
    STATIC 
        source/common/pluginview.cpp
        source/common/pluginview.h
        source/main/pluginfactory.cpp
        source/main/pluginfactory.h
        source/main/pluginfactory_constexpr.h
)

# Additional sources for VST3 SDK usage
if(VST_SDK)
    target_sources(sdk
        PRIVATE
            source/main/moduleinit.cpp
            source/main/moduleinit.h
            source/vst/utility/audiobuffers.h
            source/vst/utility/processcontextrequirements.h
            source/vst/utility/processdataslicer.h
            source/vst/utility/ringbuffer.h
            source/vst/utility/rttransfer.h
            source/vst/utility/sampleaccurate.h
            source/vst/utility/stringconvert.cpp
            source/vst/utility/stringconvert.h
            source/vst/utility/testing.cpp
            source/vst/utility/testing.h
            source/vst/utility/vst2persistence.h
            source/vst/utility/vst2persistence.cpp
            source/vst/vstaudioeffect.cpp
            source/vst/vstaudioeffect.h
            source/vst/vstbus.cpp
            source/vst/vstbus.h
            source/vst/vstbypassprocessor.h
            source/vst/vstcomponent.cpp
            source/vst/vstcomponent.h
            source/vst/vstcomponentbase.cpp
            source/vst/vstcomponentbase.h
            source/vst/vsteditcontroller.cpp
            source/vst/vsteditcontroller.h
            source/vst/vsteventshelper.h
            source/vst/vsthelpers.h
            source/vst/vstinitiids.cpp
            source/vst/vstnoteexpressiontypes.cpp
            source/vst/vstnoteexpressiontypes.h
            source/vst/vstparameters.cpp
            source/vst/vstparameters.h
            source/vst/vstrepresentation.cpp
            source/vst/vstrepresentation.h
    )
endif(VST_SDK)

target_compile_features(sdk
    PUBLIC
        cxx_std_14
)

# add dependencies
target_link_libraries(sdk 
    PUBLIC 
        sdk_common
)
target_include_directories(sdk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
smtg_target_setup_universal_binary(sdk)
get_target_property(sdk_SOURCES sdk SOURCES)

# iOS target
if(SMTG_MAC AND XCODE AND SMTG_IOS_DEVELOPMENT_TEAM)
    add_library(sdk_ios 
        STATIC 
            ${sdk_SOURCES}
    )
    smtg_set_platform_ios(sdk_ios)
    set_target_properties(sdk_ios
        PROPERTIES
            ${SDK_IDE_LIBS_FOLDER}
    )
    target_link_libraries(sdk_ios 
        PUBLIC 
            base_ios
            pluginterfaces_ios
    )
endif(SMTG_MAC AND XCODE AND SMTG_IOS_DEVELOPMENT_TEAM)

#--for hosting----------------
if(VST_SDK)
    add_library(sdk_hosting 
        STATIC
            source/vst/hosting/connectionproxy.cpp
            source/vst/hosting/connectionproxy.h
            source/vst/hosting/eventlist.cpp
            source/vst/hosting/eventlist.h
            source/vst/hosting/hostclasses.cpp
            source/vst/hosting/hostclasses.h
            source/vst/hosting/module.cpp
            source/vst/hosting/module.h
            source/vst/hosting/parameterchanges.cpp
            source/vst/hosting/parameterchanges.h
            source/vst/hosting/pluginterfacesupport.cpp
            source/vst/hosting/pluginterfacesupport.h
            source/vst/hosting/processdata.cpp
            source/vst/hosting/processdata.h
            source/vst/utility/optional.h
            source/vst/utility/stringconvert.cpp
            source/vst/utility/stringconvert.h
            source/vst/utility/uid.h
            source/vst/utility/versionparser.h
            source/vst/vstinitiids.cpp
    )
    # add dependencies
    target_link_libraries(sdk_hosting 
        PUBLIC 
            sdk_common
    )
    target_include_directories(sdk_hosting
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    smtg_target_setup_universal_binary(sdk_hosting)

    get_target_property(sdk_hosting_SOURCES sdk_hosting SOURCES)

    # iOS target for hosting
    if(SMTG_MAC AND XCODE AND SMTG_IOS_DEVELOPMENT_TEAM)
        add_library(sdk_hosting_ios STATIC 
            ${sdk_hosting_SOURCES}
        )
        smtg_target_set_platform_ios(sdk_hosting_ios)
        set_target_properties(sdk_hosting_ios
            PROPERTIES
                ${SDK_IDE_LIBS_FOLDER}
        )
        target_link_libraries(sdk_hosting_ios
            PUBLIC 
                base_ios
                pluginterfaces_ios
        )
    endif(SMTG_MAC AND XCODE AND SMTG_IOS_DEVELOPMENT_TEAM)
endif(VST_SDK)