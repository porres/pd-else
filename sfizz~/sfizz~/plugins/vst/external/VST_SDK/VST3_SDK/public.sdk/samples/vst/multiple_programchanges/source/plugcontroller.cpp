//------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/XX/source/plugcontroller.cpp
// Created by  : Steinberg, 02/2016
// Description : Plug-in Example for VST SDK 3.x using Multiple ProgramChange parameters
//
//-----------------------------------------------------------------------------
// LICENSE
// (c) 2022, Steinberg Media Technologies GmbH, All Rights Reserved
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

#include "plugcontroller.h"
#include "plugparamids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/source/vst/utility/stringconvert.h"

#include "pluginterfaces/base/futils.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstcomponent.h"

namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
// PlugController Implementation
//------------------------------------------------------------------------
tresult PLUGIN_API PlugController::initialize (FUnknown* context)
{
	tresult result = EditControllerEx1::initialize (context);
	if (result != kResultOk)
	{
		return result;
	}

	//---Create Parameters------------

	//---Bypass parameter---
	int32 stepCount = 1;
	ParamValue defaultVal = 0;
	int32 flags = ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass;
	int32 tag = kBypassId;
	parameters.addParameter (STR ("Bypass"), nullptr, stepCount, defaultVal, flags, tag);

	//---Create top root unit with kProgramId as id for the programList
	addUnit (new Unit (STR ("Root"), kRootUnitId, kNoParentUnitId));

	//---We want here kNumSlots slots with a program change per slot
	for (int32 i = 0; i < kNumSlots; i++)
	{
		int32 kSlotUnitID = kRootUnitId + i + 1;
		int32 kProgramListId = kProgramStartId + i;

		// create a unit for the slot associated to a programList created below
		std::u16string slotName = STR ("Slot ");
		slotName += VST3::toString (i + 1);
		addUnit (new Unit (slotName.data (), kSlotUnitID, kRootUnitId, kProgramListId));

		// create the program list: here kNumProgs entries attached to the previous created
		// unit/slot
		std::u16string programListName = STR ("Bank ");
		programListName += VST3::toString (i + 1);
		auto* prgList = new ProgramList (programListName.data (), kProgramListId, kSlotUnitID);
		for (int32 i = 0; i < kNumProgs; i++)
		{
			std::u16string title = STR ("Prog ");
			title += VST3::toString (i + 1);
			prgList->addProgram (title.data ());
		}
		addProgramList (prgList);

		//---Program Change parameter---
		Parameter* prgParam = prgList->getParameter ();

		// by default this program change parameter if automatable we can overwrite this:
		prgParam->getInfo ().flags &= ~ParameterInfo::kCanAutomate;

		// add this programChange parameter to the exported parameter list
		parameters.addParameter (prgParam);
	}

	//---Gain parameter---
	parameters.addParameter (STR ("Gain"), nullptr, 0, 1.f, ParameterInfo::kCanAutomate, kGainId);

	return result;
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API PlugController::setParamNormalized (ParamID tag, ParamValue value)
{
	tresult res = EditControllerEx1::setParamNormalized (tag, value);
	if (res == kResultOk && tag >= kProgramStartId && tag < kProgramEndId) // program changes
	{
		// here we use the program value as gain...just an example
		value *= (ParamValue) (tag - kProgramStartId + 1) /
		         (ParamValue) (kProgramEndId - kProgramStartId);
		EditControllerEx1::setParamNormalized (kGainId, value);

		componentHandler->restartComponent (kParamValuesChanged);
	}
	return res;
}

//------------------------------------------------------------------------
tresult PLUGIN_API PlugController::getUnitByBus (MediaType type, BusDirection dir, int32 busIndex,
                                                 int32 channel, UnitID& unitId)
{
	if (type == kEvent && dir == kInput && busIndex == 0 && channel >= 0 && channel < kNumSlots)
	{
		unitId = kRootUnitId + channel + 1;

		return kResultTrue;
	}

	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API PlugController::setComponentState (IBStream* state)
{
	// we receive the current state of the component (processor part)
	// we read only the gain and bypass value...
	if (!state)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	// read the bypass
	int32 bypassState = 0;
	if (streamer.readInt32 (bypassState) == false)
		return kResultFalse;
	setParamNormalized (kBypassId, bypassState ? 1 : 0);

	// read number of program
	int32 numPrograms = 0;
	if (streamer.readInt32 (numPrograms) == false)
		return kResultFalse;

	// read the program
	for (int32 i = 0; i < numPrograms; i++)
	{
		int32 programState = 0;
		if (streamer.readInt32 (programState) == false)
			return kResultFalse;

		if (i < kNumSlots)
			EditControllerEx1::setParamNormalized (
			    kProgramStartId + i, ToNormalized<ParamValue> (programState, kNumProgs - 1));
	}

	// read the Gain param
	float savedGain = 0.f;
	if (streamer.readFloat (savedGain) == false)
		return kResultFalse;
	setParamNormalized (kGainId, savedGain);

	return kResultOk;
}
}
} // namespaces
