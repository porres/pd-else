//------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/panner/source/plugcontroller.cpp
// Created by  : Steinberg, 02/2020
// Description : Panner Example for VST 3
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

#include "../include/plugcontroller.h"
#include "../include/plugids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/source/vst/utility/stringconvert.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"

#include <string_view>

using namespace VSTGUI;

namespace Steinberg {
namespace Panner {

// example of custom parameter (overwriting to and fromString)
//------------------------------------------------------------------------
class PanParameter : public Vst::Parameter
{
public:
	PanParameter (int32 flags, int32 id);

	void toString (Vst::ParamValue normValue, Vst::String128 string) const SMTG_OVERRIDE;
	bool fromString (const Vst::TChar* string, Vst::ParamValue& normValue) const SMTG_OVERRIDE;
};

//------------------------------------------------------------------------
// PanParameter Implementation
//------------------------------------------------------------------------
PanParameter::PanParameter (int32 flags, int32 id)
{
	Steinberg::UString (info.title, USTRINGSIZE (info.title)).assign (USTRING ("Pan"));
	Steinberg::UString (info.units, USTRINGSIZE (info.units)).assign (USTRING (""));

	info.flags = flags;
	info.id = id;
	info.stepCount = 0;
	info.defaultNormalizedValue = 0.5f;
	info.unitId = Vst::kRootUnitId;

	setNormalized (.5f);
}

//------------------------------------------------------------------------
void PanParameter::toString (Vst::ParamValue normValue, Vst::String128 string) const
{
	char text[32];
	if (normValue >= 0.505)
	{
		snprintf (text, 32, "R %d", int32 ((normValue - 0.5f) * 200 + 0.5f));
	}
	else if (normValue <= 0.495)
	{
		snprintf (text, 32, "L %d", int32 ((0.5f - normValue) * 200 + 0.5f));
	}
	else
	{
		strcpy (text, "C");
	}

	Steinberg::UString (string, 128).fromAscii (text);
}

//------------------------------------------------------------------------
bool PanParameter::fromString (const Vst::TChar* string, Vst::ParamValue& normValue) const
{
	std::u16string_view stringView (string);
	auto pos = stringView.find_first_of (u"C");
	if (pos != std::string::npos)
	{
		normValue = 0.5;
		return true;
	}
	else
	{
		bool left = stringView.find_first_of (u"L") == 0;
		bool right = stringView.find_first_of (u"R") == 0;
		if (left || right)
			stringView = {stringView.data () + 1, stringView.size () - 1};
		auto string8 = VST3::StringConvert::convert (stringView.data ());
		char* end = nullptr;
		double tmp = strtod (string8.data (), &end);
		if (end != string8.data ())
		{
			if (tmp < 0)
			{
				left = true;
				if (tmp < -100)
					tmp = 100;
				else
					tmp = -tmp;
			}
			else if (tmp > 100.0)
			{
				normValue = 1;
				return true;
			}
			if (!left)
				normValue = tmp / 200 + 0.5;
			else
				normValue = 0.5 - tmp / 200;

			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API PlugController::initialize (FUnknown* context)
{
	tresult result = EditController::initialize (context);
	if (result == kResultTrue)
	{
		//---Create Parameters------------
		parameters.addParameter (STR16 ("Bypass"), nullptr, 1, 0,
		                         Vst::ParameterInfo::kCanAutomate | Vst::ParameterInfo::kIsBypass,
		                         PannerParams::kBypassId);

		auto* panParam = new PanParameter (Vst::ParameterInfo::kCanAutomate, PannerParams::kParamPanId);
		parameters.addParameter (panParam);
	}
	return kResultTrue;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API PlugController::createView (const char* _name)
{
	std::string_view name (_name);
	if (name == Vst::ViewType::kEditor)
	{
		auto* view = new VST3Editor (this, "view", "plug.uidesc");
		return view;
	}
	return nullptr;
}

//------------------------------------------------------------------------
tresult PLUGIN_API PlugController::getParameterIDFromFunctionName (Vst::UnitID unitID,
                                                                   FIDString functionName,
                                                                   Vst::ParamID& paramID)
{
	using namespace Vst;

	paramID = kNoParamId;

	if (unitID == kRootUnitId && FIDStringsEqual (functionName, FunctionNameType::kPanPosCenterX))
		paramID = PannerParams::kParamPanId;

	return (paramID != kNoParamId) ? kResultOk : kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API PlugController::setComponentState (IBStream* state)
{
	// we receive the current state of the component (processor part)
	// we read our parameters and bypass value...
	if (!state)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	float savedParam1 = 0.f;
	if (streamer.readFloat (savedParam1) == false)
		return kResultFalse;
	setParamNormalized (PannerParams::kParamPanId, savedParam1);

	// read the bypass
	int32 bypassState;
	if (streamer.readInt32 (bypassState) == false)
		return kResultFalse;
	setParamNormalized (kBypassId, bypassState ? 1 : 0);

	return kResultOk;
}

//------------------------------------------------------------------------
} // namespace
} // namespace Steinberg
