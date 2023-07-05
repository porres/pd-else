
function(smtg_target_setup_options target)
    set(options)
    set(oneValueArgs 
        BUNDLE_IDENTIFIER 
        COMPANY_NAME 
    )
    set(multiValueArgs)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "${CMAKE_CURRENT_FUNCTION}: The following parameters are unrecognized: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT ARG_BUNDLE_IDENTIFIER)
        message(FATAL_ERROR "${CMAKE_CURRENT_FUNCTION}: BUNDLE_IDENTIFIER must be specified")
    endif()

    if(NOT ARG_COMPANY_NAME)
        message(FATAL_ERROR "${CMAKE_CURRENT_FUNCTION}: ARG_COMPANY_NAME must be specified")
    endif()

    smtg_target_configure_version_file(${target})

    if(SMTG_MAC)
        smtg_target_set_bundle(${target}
            BUNDLE_IDENTIFIER "${ARG_BUNDLE_IDENTIFIER}"
            COMPANY_NAME "${ARG_COMPANY_NAME}"
        )
    elseif(SMTG_WIN)
        target_sources(${target}
            PRIVATE 
                resource/info.rc
        )
    endif()

    target_link_libraries(${target}
        PRIVATE
            sdk
    )
endfunction()

function(smtg_target_setup_as_vst3_example target)
    set_target_properties(${target}
        PROPERTIES 
            ${SDK_IDE_PLUGIN_EXAMPLES_FOLDER}
    )

    target_compile_features(${target}
        PUBLIC
            cxx_std_17
    )

    smtg_target_setup_options(${target}
        BUNDLE_IDENTIFIER "com.steinberg.vst3.${target}"
        COMPANY_NAME "Steinberg Media Technologies"
    )
endfunction()

if(ANDROID)
    add_subdirectory(adelay)
    return()
endif(ANDROID)

include(SMTG_AddSubDirectories)

smtg_add_subdirectories()
