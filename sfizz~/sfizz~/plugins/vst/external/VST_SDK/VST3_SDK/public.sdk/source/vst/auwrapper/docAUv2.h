//------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/auwrapper/docAUv2.h
// Created by  : Steinberg, 07/2017
// Description : VST 3 -> AU Wrapper
//
//-----------------------------------------------------------------------------
// LICENSE
// (c) 2023, Steinberg Media Technologies GmbH, All Rights Reserved
//-----------------------------------------------------------------------------
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
//   * Redistributions of source code must retain the above copyright notice, 
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation 
//     and/or other materials provided with the distribution.
//   * Neither the name of the Steinberg Media Technologies nor the names of its
//     contributors may be used to endorse or promote products derived from this 
//     software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

/**
*******************************************
\page AUWrapper VST 3 - Audio Unit Wrapper
*******************************************
\tableofcontents

\brief
Helper Class wrapping a <b>VST 3 plug-in</b> to an Audio Unit v2 plug-in

***************************
\section AUIntroduction Introduction
***************************
The VST 3 SDK comes with an AudioUnit wrapper, which wraps one VST 3 Audio Processor and Edit Controller as an AudioUnit effect/instrument.

The wrapper is a small dynamic library which loads the <b>VST 3 plug-in</b>. No extra code is needed to be written.
\n

***************************
\section AUhowdoesitwork How does it work?
***************************
to add an AUv2 to your VST3 plug-in you have to use cmake and call the cmake function:

@code
	#------------------------------------------------------------------------
	# Add an AudioUnit Version 2 target for macOS
	# @param target                 target name
	# @param bundle_name			name of the bundle
	# @param bundle_identifier		bundle identifier
	# @param info_plist_template	the info.plist file containing the needed AudioUnit keys
	# @param vst3_plugin_target     the vst3 plugin target
	#------------------------------------------------------------------------
	function(smtg_target_add_auv2 target bundle_name bundle_identifier info_plist_template vst3_plugin_target)
@endcode

This adds a new target to your project which builds and setup the AudioUnit. After successfully building the AU should be copied to your 
user Library folder (Library/Audio/Components/) and ready to be used.

*/
