
if(SMTG_ADD_VST3_HOSTING_SAMPLES)
 
    # Enable Sample audioHost (based on Jack Audio)
    if(SMTG_LINUX)
        option(SMTG_ENABLE_USE_OF_JACK "Enable Use of Jack" ON)
    else()
        # not yet tested on Windows and Mac
        option(SMTG_ENABLE_USE_OF_JACK "Enable Use of Jack" OFF)
    endif(SMTG_LINUX)

    if(SMTG_ENABLE_USE_OF_JACK)
        include(SMTG_FindJack)

        if(${LIBJACK_FOUND})
            set(audiohost_sources
                ${SDK_ROOT}/public.sdk/samples/vst-hosting/editorhost/source/platform/iapplication.h
                ${SDK_ROOT}/public.sdk/source/vst/hosting/connectionproxy.cpp
                ${SDK_ROOT}/public.sdk/source/vst/hosting/connectionproxy.h
                ${SDK_ROOT}/public.sdk/source/vst/hosting/eventlist.cpp
                ${SDK_ROOT}/public.sdk/source/vst/hosting/eventlist.h
                ${SDK_ROOT}/public.sdk/source/vst/hosting/hostclasses.cpp
                ${SDK_ROOT}/public.sdk/source/vst/hosting/hostclasses.h
                ${SDK_ROOT}/public.sdk/source/vst/hosting/module.cpp
                ${SDK_ROOT}/public.sdk/source/vst/hosting/module.h
                ${SDK_ROOT}/public.sdk/source/vst/hosting/parameterchanges.cpp
                ${SDK_ROOT}/public.sdk/source/vst/hosting/parameterchanges.h 
                ${SDK_ROOT}/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
                ${SDK_ROOT}/public.sdk/source/vst/hosting/pluginterfacesupport.h
                ${SDK_ROOT}/public.sdk/source/vst/hosting/plugprovider.cpp
                ${SDK_ROOT}/public.sdk/source/vst/hosting/plugprovider.h
                ${SDK_ROOT}/public.sdk/source/vst/hosting/processdata.cpp
                ${SDK_ROOT}/public.sdk/source/vst/hosting/processdata.h
                ${SDK_ROOT}/public.sdk/source/vst/utility/optional.h
                ${SDK_ROOT}/public.sdk/source/vst/utility/stringconvert.cpp
                ${SDK_ROOT}/public.sdk/source/vst/utility/stringconvert.h
                ${SDK_ROOT}/public.sdk/source/vst/utility/uid.h
                ${SDK_ROOT}/public.sdk/source/vst/vstinitiids.cpp
                source/audiohost.cpp
                source/audiohost.h
                source/media/audioclient.cpp
                source/media/audioclient.h
                source/media/imediaserver.h
                source/media/iparameterclient.h
                source/media/jack/jackclient.cpp
                source/media/miditovst.h
                source/platform/appinit.h
                source/usediids.cpp
            )

            if(SMTG_MAC)
                set(audiohost_sources 
                    ${audiohost_sources}
                    ${SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm
                )
                set(audiohost_PLATFORM_LIBS "-framework Cocoa")
                get_filename_component(InfoPlistFile "resource/Info.plist" ABSOLUTE)
                set(APP_PROPERTIES
                    MACOSX_BUNDLE TRUE
                    MACOSX_BUNDLE_INFO_PLIST ${InfoPlistFile}
                )
            elseif(SMTG_LINUX)
                set(audiohost_sources
                    ${audiohost_sources}
                    ${SDK_ROOT}/public.sdk/source/vst/hosting/module_linux.cpp
                )
                set(audiohost_PLATFORM_LIBS)
                set(APP_PROPERTIES)
            elseif(SMTG_WIN)
                set(audiohost_sources
                    ${audiohost_sources}
                    ${SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp
                )
                set(audiohost_PLATFORM_LIBS)
                if(MSVC)
                    set(APP_PROPERTIES
                        LINK_FLAGS
                            "/SUBSYSTEM:console"
                    )
                endif(MSVC)
            endif(SMTG_MAC)

            set(target audiohost)

            add_executable(${target} ${audiohost_sources})
            set_target_properties(${target}
                PROPERTIES
                    ${SDK_IDE_HOSTING_EXAMPLES_FOLDER}
            )
            target_compile_features(${target}
                PUBLIC
                    cxx_std_14
            )
            target_link_libraries(${target} 
                PRIVATE
                    base sdk
                    ${audiohost_PLATFORM_LIBS}
                    ${LIBJACK_LIBRARY}
            )
            target_include_directories(${target}
                PRIVATE
                    ${LIBJACK_INCLUDE_DIR}
            )
            if (APP_PROPERTIES)
                set_target_properties(${target}
                    PROPERTIES
                        ${APP_PROPERTIES}
                )
            endif(APP_PROPERTIES)
        else()
            message(STATUS "[SMTG] LIBJACK_FOUND is not set.")  
        endif(${LIBJACK_FOUND})

    endif(SMTG_ENABLE_USE_OF_JACK)
endif(SMTG_ADD_VST3_HOSTING_SAMPLES)
