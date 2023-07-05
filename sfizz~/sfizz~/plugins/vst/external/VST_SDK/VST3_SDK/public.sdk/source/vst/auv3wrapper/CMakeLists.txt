include(SMTG_AddVST3AuV3)

# iOS target
if(SMTG_MAC)
    if(XCODE)
        set(auv3wrapperlib_sources
            ${SDK_ROOT}/public.sdk/source/vst/auv3wrapper/Shared/AUv3Wrapper.mm
            ${SDK_ROOT}/public.sdk/source/vst/auv3wrapper/Shared/AUv3Wrapper.h
            ${SDK_ROOT}/public.sdk/source/vst/auv3wrapper/Shared/AUv3AudioEngine.mm
            ${SDK_ROOT}/public.sdk/source/vst/auv3wrapper/Shared/AUv3AudioEngine.h
            ${SDK_ROOT}/public.sdk/source/vst/auwrapper/NSDataIBStream.mm
            ${SDK_ROOT}/public.sdk/source/vst/auwrapper/NSDataIBStream.h
            ${SDK_ROOT}/public.sdk/source/vst/utility/mpeprocessor.cpp
            ${SDK_ROOT}/public.sdk/source/vst/utility/mpeprocessor.h
        )

        #--------------------------------------------------------------------------------------------------------
        set(target auv3wrappermacos)
        add_library(${target}
            STATIC 
                ${auv3wrapperlib_sources}
        )
        set_target_properties(${target} 
            PROPERTIES 
                ${SDK_IDE_LIBS_FOLDER} XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
        )
        target_compile_features(${target}
            PUBLIC
                cxx_std_14
        )
        target_link_libraries(${target} 
            PRIVATE 
                sdk_hosting
        )
        if (SMTG_AUV3_WRAPPER_EXTERNAL_PLUGIN_FACTORY)
            target_compile_definitions(${target}
                PRIVATE 
                    SMTG_AUV3_WRAPPER_EXTERNAL_PLUGIN_FACTORY=1)
        endif()

        #--------------------------------------------------------------------------------------------------------
        if(SMTG_IOS_DEVELOPMENT_TEAM)
            set(target auv3wrapperios)
            add_library(${target}
                STATIC
                    ${auv3wrapperlib_sources}
            )
            set_target_properties(${target}
                PROPERTIES 
                    ${SDK_IDE_LIBS_FOLDER} XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
            )
            set_target_properties(${target}
                PROPERTIES
                    LINK_FLAGS "-Wl,-F/Library/Frameworks"
            )
            smtg_set_platform_ios(${target})
            target_compile_features(${target}
                PUBLIC
                    cxx_std_14
            )
            target_link_libraries(${target}
                PRIVATE
                    sdk_hosting_ios
            )
            if (SMTG_AUV3_WRAPPER_EXTERNAL_PLUGIN_FACTORY)
                target_compile_definitions(${target}
                    PRIVATE
                        SMTG_AUV3_WRAPPER_EXTERNAL_PLUGIN_FACTORY=1
                )
            endif()
        endif()
    else()
        message("* To enable building the AUv3 Wrapper example for iOS you need to set the SMTG_IOS_DEVELOPMENT_TEAM and use the Xcode generator")
    endif()
endif()
