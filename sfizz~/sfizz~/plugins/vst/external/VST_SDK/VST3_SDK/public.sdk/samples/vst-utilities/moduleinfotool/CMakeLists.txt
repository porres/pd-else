
set(moduleinfotool_sources
    source/main.cpp
	../../../source/vst/moduleinfo/json.h
	../../../source/vst/moduleinfo/jsoncxx.h
	../../../source/vst/moduleinfo/moduleinfo.h
	../../../source/vst/moduleinfo/moduleinfocreator.cpp
	../../../source/vst/moduleinfo/moduleinfocreator.h
	../../../source/vst/moduleinfo/moduleinfoparser.cpp
	../../../source/vst/moduleinfo/moduleinfoparser.h
)

if(SMTG_MAC)
    set(moduleinfotool_sources
        ${moduleinfotool_sources}
        ${SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm
    )
    set_source_files_properties(
        "${SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm" PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
elseif(SMTG_LINUX)
    set(moduleinfotool_sources
        ${moduleinfotool_sources}
        ${SDK_ROOT}/public.sdk/source/vst/hosting/module_linux.cpp
    )
elseif(SMTG_WIN)
    set(moduleinfotool_sources
        ${moduleinfotool_sources}
        ${SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp
    )
endif(SMTG_MAC)

set(target moduleinfotool)
add_executable(${target} ${moduleinfotool_sources})

if(SDK_IDE_UTILITIES_FOLDER)
    set_target_properties(${target}
        PROPERTIES
            ${SDK_IDE_UTILITIES_FOLDER}
    )
endif(SDK_IDE_UTILITIES_FOLDER)

target_compile_features(${target}
    PUBLIC
        cxx_std_17
)
target_link_libraries(${target}
    PRIVATE
        sdk_hosting
)
smtg_target_codesign(${target})
smtg_target_setup_universal_binary(${target})
if(APPLE AND NOT XCODE)
    find_library(COCOA_FRAMEWORK Cocoa)
    target_link_libraries(${target}
        PRIVATE
            ${COCOA_FRAMEWORK}
    )
endif(APPLE AND NOT XCODE)
