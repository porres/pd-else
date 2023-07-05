//------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : VST SDK
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/again_sampleaccurate/source/agsa_controller.cpp
// Created by  : Steinberg, 04/2021
// Description : AGain with Sample Accurate Parameter Changes
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
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#include "agsa.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "base/source/fstreamer.h"

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {
namespace AgainSampleAccurate {

//------------------------------------------------------------------------
class Controller : public EditController
{
	tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;
	tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	tresult PLUGIN_API setComponentState (IBStream* state) SMTG_OVERRIDE;
};

//------------------------------------------------------------------------
tresult PLUGIN_API Controller::initialize (FUnknown* context)
{
	tresult result = EditController::initialize (context);
	if (result != kResultOk)
	{
		return result;
	}
	parameters.addParameter (STR ("Bypass"), nullptr, 1, 0.,
	                         ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass,
	                         ParameterID::Bypass);
	parameters.addParameter (STR ("Gain"), STR ("%"), 0, 1., ParameterInfo::kCanAutomate,
	                         ParameterID::Gain);
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Controller::terminate ()
{
	return EditController::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API Controller::setComponentState (IBStream* state)
{
	if (!state)
		return kInvalidArgument;

	IBStreamer streamer (state, kLittleEndian);

	uint32 numParams;
	if (streamer.readInt32u (numParams) == false)
		return kResultFalse;

	ParamID pid;
	ParamValue value;
	for (uint32 i = 0u; i < numParams; ++i)
	{
		if (!streamer.readInt32u (pid))
			break;
		if (!streamer.readDouble (value))
			break;
		if (auto param = parameters.getParameter (pid))
			param->setNormalized (value);
	}
	return kResultTrue;
}

//------------------------------------------------------------------------
FUnknown* createControllerInstance (void*)
{
	return static_cast<IEditController*> (new Controller);
}

//------------------------------------------------------------------------
} // AgainSampleAccurate
} // Vst
} // Steinberg
