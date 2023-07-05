
# pluginterfaces should actually be a header-only library,
# but it has some sources as well which need compilation.

cmake_minimum_required(VERSION 3.19)

project(smtg_pluginterfaces)

# base interfaces
add_library(pluginterfaces STATIC
    base/conststringtable.cpp
    base/conststringtable.h
    base/coreiids.cpp
    base/falignpop.h
    base/falignpush.h
    base/fplatform.h
    base/fstrdefs.h
    base/ftypes.h
    base/funknown.cpp
    base/funknown.h
    base/funknownimpl.h
    base/futils.h
    base/fvariant.h
    base/geoconstants.h
    base/ibstream.h
    base/icloneable.h
    base/ierrorcontext.h
    base/ipersistent.h
    base/ipluginbase.h
    base/istringresult.h
    base/iupdatehandler.h
    base/keycodes.h
    base/pluginbasefwd.h
    base/smartpointer.h
    base/typesizecheck.h
    base/ucolorspec.h
    base/ustring.cpp
    base/ustring.h
)

# check for C11 atomic header
include(CheckSourceCompiles)
set(SMTG_CHECK_STDATOMIC_H_SRC
"#include <stdatomic.h>
int main () { 
	atomic_int_least32_t value = 0;
	atomic_fetch_add (&value, 1); 
	return 0; 
}"
)
#set(CMAKE_REQUIRED_QUIET 1)
check_source_compiles(CXX "${SMTG_CHECK_STDATOMIC_H_SRC}" SMTG_USE_STDATOMIC_H)
if(SMTG_USE_STDATOMIC_H)
    target_compile_definitions(pluginterfaces
        PRIVATE
            "SMTG_USE_STDATOMIC_H=${SMTG_USE_STDATOMIC_H}"
    )
endif(SMTG_USE_STDATOMIC_H)

# vst3 interfaces
if (VST_SDK)
    target_sources(pluginterfaces
        PRIVATE
            gui/iplugview.h
            gui/iplugviewcontentscalesupport.h
            vst/ivstattributes.h
            vst/ivstaudioprocessor.h
            vst/ivstautomationstate.h
            vst/ivstchannelcontextinfo.h
            vst/ivstcomponent.h
            vst/ivstcontextmenu.h
            vst/ivsteditcontroller.h
            vst/ivstevents.h
            vst/ivsthostapplication.h
            vst/ivstinterappaudio.h
            vst/ivstmessage.h
            vst/ivstmidicontrollers.h
            vst/ivstmidilearn.h
            vst/ivstnoteexpression.h
            vst/ivstparameterchanges.h
            vst/ivstparameterfunctionname.h
            vst/ivstphysicalui.h
            vst/ivstpluginterfacesupport.h
            vst/ivstplugview.h
            vst/ivstprefetchablesupport.h
            vst/ivstprocesscontext.h
            vst/ivstrepresentation.h
            vst/ivstunits.h
            vst/vstpresetkeys.h
            vst/vstpshpack4.h
            vst/vstspeaker.h
            vst/vsttypes.h
)
endif()

# ski interfaces
if (SKI_SDK)
    target_sources(pluginterfaces
        PRIVATE
            host/paramids.cpp 
            host/paramids.h
    )
endif()

target_include_directories(pluginterfaces PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(pluginterfaces
    PUBLIC
        cxx_std_11
)

smtg_target_setup_universal_binary(pluginterfaces)

# iOS target
if(SMTG_MAC AND XCODE AND SMTG_IOS_DEVELOPMENT_TEAM)
	get_target_property(PLUGINTERFACES_SOURCES pluginterfaces SOURCES)
	add_library(pluginterfaces_ios STATIC ${PLUGINTERFACES_SOURCES})
	smtg_target_set_platform_ios(pluginterfaces_ios)
endif()
