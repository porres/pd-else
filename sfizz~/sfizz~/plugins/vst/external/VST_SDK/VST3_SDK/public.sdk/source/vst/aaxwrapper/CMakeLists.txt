
set(target aaxwrapper)
set(${target}_sources
    ${SDK_ROOT}/public.sdk/source/vst/basewrapper/basewrapper.sdk.cpp
    aaxentry.cpp
    aaxlibrary.cpp
    aaxwrapper.cpp
    aaxwrapper.h
    aaxwrapper_description.h
    aaxwrapper_gui.cpp
    aaxwrapper_gui.h
    aaxwrapper_parameters.cpp
    aaxwrapper_parameters.h
    docaax.h
    resource/PlugIn.ico
)
add_library(${target} STATIC ${${target}_sources})

target_include_directories(${target}
    PRIVATE
        "${SMTG_AAX_SDK_PATH}/Interfaces"
        "${SMTG_AAX_SDK_PATH}/Interfaces/ACF"
        "${SMTG_AAX_SDK_PATH}/Libs/AAXLibrary/Include"
)
target_link_libraries(${target}
    PRIVATE
        base
)
smtg_target_setup_universal_binary(${target})

target_compile_features(aaxwrapper
    PUBLIC
        cxx_std_11
)

if(XCODE)
    add_compile_options(-Wno-incompatible-ms-struct)
elseif(SMTG_WIN)
    # too much warnings in the AAX SDK!!
    add_compile_options(/wd4996)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/GR)
    if(MSVC)
        target_compile_options(${target} 
            PRIVATE 
                /wd4127 /wd6001 /wd6053
        )
    endif(MSVC)
endif(XCODE)
