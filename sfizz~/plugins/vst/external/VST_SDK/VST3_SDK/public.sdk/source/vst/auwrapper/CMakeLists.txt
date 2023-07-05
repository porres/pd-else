if(SMTG_MAC)
    if (XCODE AND SMTG_COREAUDIO_SDK_PATH)

		string(RANDOM LENGTH 20 CocoaId)
		file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/au/aucocoaclassprefix.h"
			CONTENT "#define SMTG_AUCocoaUIBase_CLASS_NAME SMTG_AUCocoaUIBase_${CocoaId}"
		)

        set(target auwrapper)
        set(${target}_sources
            aucarbonview.mm
            aucarbonview.h
            aucocoaview.mm
            aucocoaview.h
            ausdk.mm
            auwrapper.mm
            auwrapper.h
            NSDataIBStream.mm
            NSDataIBStream.h
        )
        add_library(${target}
            STATIC
                ${${target}_sources}
        )
        smtg_target_setup_universal_binary(${target})
        set_target_properties(${target}
            PROPERTIES
                ${SDK_IDE_LIBS_FOLDER}
        )
		target_compile_features(${target}
			PUBLIC
				cxx_std_17
		)

        target_link_libraries(${target} 
            PRIVATE 
                sdk_hosting 
                "-framework AudioUnit" "-framework CoreMIDI"
                "-framework AudioToolbox"
                "-framework CoreFoundation"
                "-framework Carbon" 
                "-framework Cocoa" 
                "-framework CoreAudio"
        )
        target_include_directories(${target}
            PRIVATE 
                "${SMTG_COREAUDIO_SDK_PATH}/**"
                "${CMAKE_CURRENT_BINARY_DIR}/au/"
        )
    else()
        message("[SMTG] * To enable building the AudioUnit wrapper, you need to use the Xcode generator and set SMTG_COREAUDIO_SDK_PATH to the path of your installation of the CoreAudio SDK!")
    endif(XCODE AND SMTG_COREAUDIO_SDK_PATH)
endif(SMTG_MAC)