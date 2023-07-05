include(SMTG_AddVST3AuV3)

if(SMTG_MAC AND SMTG_ADD_VSTGUI)
    if(XCODE)
        set(notexpsynth_mac_app_sources
            "macOS/Sources/WindowController.m"
            "macOS/Sources/ViewController.m"
            "macOS/Sources/ViewController.h"
            "macOS/Sources/AppDelegate.m"
            "macOS/Sources/AppDelegate.h"
        )

        set(notexpsynth_ios_app_sources
            "iOS/Sources/ViewController.m"
            "iOS/Sources/ViewController.h"
            "iOS/Sources/AppDelegate.m"
            "iOS/Sources/AppDelegate.h"
            "iOS/Sources/main.mm"
        )

        set(notexpsynth_mac_app_ui_resources
            "macOS/Resources/Base.lproj/Main.storyboard"
            "macOS/Resources/NoteExpressionSynth.icns"
        )
        
        set(notexpsynth_ios_app_ui_resources
            "iOS/Resources/Base.lproj/LaunchScreen.storyboard"
            "iOS/Resources/Base.lproj/Main.storyboard"
            "iOS/Resources/Assets.xcassets"
        )
        
        #--------------------------------------------------------------------------------------------------------
        # macOS targets
        #--------------------------------------------------------------------------------------------------------
        smtg_add_auv3_app(notexpsynth_auv3_macos
            "macOS"
            "notexpsynth AUV3 macOS"
            "com.steinberg.sdk.auv3.notexpsynthmac"
            "audiounitconfig.h"
            "macOS/noteexpressionsynth.entitlements"
            "${notexpsynth_mac_app_sources}"
            "${notexpsynth_mac_app_ui_resources}"
            "macOS/Resources/Info.plist"
            "Shared/Info.plist"
            noteexpressionsynth
        )

        #--------------------------------------------------------------------------------------------------------
        # iOS targets
        #--------------------------------------------------------------------------------------------------------
        if(SMTG_IOS_DEVELOPMENT_TEAM)
            smtg_add_auv3_app(notexpsynth_auv3_ios
                "iOS"
                "notexpsynth AUV3 iOS"
                "com.steinberg.sdk.auv3.notexpsynthios"
                "audiounitconfig.h"
                "iOS/noteexpressionsynth.entitlements"
                "${notexpsynth_ios_app_sources}"
                "${notexpsynth_ios_app_ui_resources}"
                "iOS/Resources/Info.plist"
                "Shared/Info.plist"
                noteexpressionsynth_ios
            )

            set_target_properties(${AUV3_APP_TARGET} PROPERTIES
                XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
            )
        endif(SMTG_IOS_DEVELOPMENT_TEAM)
    endif(XCODE)
endif(SMTG_MAC AND SMTG_ADD_VSTGUI)
