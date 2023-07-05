
if(TARGET vstgui AND TARGET vstgui_standalone)

    ##########################################################################################
    set(target VST3Inspector)

    ##########################################################################################
    set(${target}_sources
        "source/app.cpp"
        "source/window.cpp"
        "source/window.h"
        "${SDK_ROOT}/pluginterfaces/base/coreiids.cpp"
        "${SDK_ROOT}/public.sdk/source/vst/hosting/module.h"
        "${SDK_ROOT}/public.sdk/source/vst/hosting/module.cpp"
        "${SDK_ROOT}/public.sdk/source/vst/utility/stringconvert.h"
        "${SDK_ROOT}/public.sdk/source/vst/utility/stringconvert.cpp"
        "${SDK_ROOT}/public.sdk/source/vst/moduleinfo/moduleinfoparser.cpp"
        "${SDK_ROOT}/public.sdk/source/vst/moduleinfo/moduleinfocreator.cpp"
    )

    set(${target}_resources
        "resource/window.uidesc"
    )

    ##########################################################################################
    if(SMTG_MAC)
        set(${target}_sources 
            ${${target}_sources}
            "${SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm"
        )
        set_source_files_properties(
            "${SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm" PROPERTIES
            COMPILE_FLAGS "-fobjc-arc"
        )
    elseif(SMTG_WIN)
        set(${target}_sources 
            ${${target}_sources}
            "${SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp"
        )
    elseif(SMTG_LINUX)
        set(${target}_sources 
            ${${target}_sources}
            "${SDK_ROOT}/public.sdk/source/vst/hosting/module_linux.cpp"
        )
    endif(SMTG_MAC)

    ##########################################################################################
	# 'vstgui_add_executable' links both targets 'vstgui' and 'vstgui_standalone' to 'VST3Inspector'
    vstgui_add_executable(${target} "${${target}_sources}")
    vstgui_set_target_bundle_id(${target} "com.steinberg.vstsdk.vst3inspector")

    if(NOT SMTG_MAC)
        set_target_properties(${target}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY $<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/bin/Debug/>$<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/bin/Release/>$<$<CONFIG:ReleaseLTO>:${CMAKE_BINARY_DIR}/bin/ReleaseLTO/>/VST3Inspector
        )
    endif(NOT SMTG_MAC)

    vstgui_add_resources(${target} "${${target}_resources}")
    vstgui_set_target_infoplist(${target} "resource/Info.plist")
    #vstgui_set_cxx_version(${target} 17)

    target_include_directories(${target}
        PRIVATE
            "${SMTG_VSTGUI_SOURCE_DIR}/"
            "${SDK_ROOT}"
    )

    if(SDK_IDE_HOSTING_EXAMPLES_FOLDER)
        set_target_properties(${target}
            PROPERTIES
                ${SDK_IDE_HOSTING_EXAMPLES_FOLDER}
        )
    endif(SDK_IDE_HOSTING_EXAMPLES_FOLDER)

endif()
