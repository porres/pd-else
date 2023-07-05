
if(ANDROID)
    return()
endif(ANDROID)

# Add VST3 Hosting Samples
if(NOT DEFINED SMTG_ADD_VST3_UTILITIES)
    option(SMTG_ADD_VST3_UTILITIES "Build VST3 Utilities" ON)
endif(NOT DEFINED SMTG_ADD_VST3_UTILITIES)

if(SMTG_ADD_VST3_UTILITIES)
	include(SMTG_AddSubDirectories)
	smtg_add_subdirectories()
endif(SMTG_ADD_VST3_UTILITIES)
