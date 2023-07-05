//-----------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/hosting/test/pluginterfacesupporttest.cpp
// Created by  : Steinberg, 08/2021
// Description : Test pluginterface support helper
// Flags       : clang-format SMTGSequencer
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

#include "public.sdk/source/main/moduleinit.h"
#include "public.sdk/source/vst/hosting/pluginterfacesupport.h"
#include "public.sdk/source/vst/utility/testing.h"

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstunits.h"

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {
namespace {

//------------------------------------------------------------------------
ModuleInitializer PlugInterfaceSupportTests ([] () {
	constexpr auto TestSuiteName = "PlugInterfaceSupport";
	registerTest (TestSuiteName, STR ("Initial interfaces"), [] (ITestResult* testResult) {
		PlugInterfaceSupport pis;
		//---VST 3.0.0--------------------------------
		EXPECT_EQ (pis.isPlugInterfaceSupported (IComponent::iid), kResultTrue);
		EXPECT_EQ (pis.isPlugInterfaceSupported (IAudioProcessor::iid), kResultTrue);
		EXPECT_EQ (pis.isPlugInterfaceSupported (IEditController::iid), kResultTrue);
		EXPECT_EQ (pis.isPlugInterfaceSupported (IConnectionPoint::iid), kResultTrue);
		EXPECT_EQ (pis.isPlugInterfaceSupported (IUnitInfo::iid), kResultTrue);
		EXPECT_EQ (pis.isPlugInterfaceSupported (IUnitData::iid), kResultTrue);
		EXPECT_EQ (pis.isPlugInterfaceSupported (IProgramListData::iid), kResultTrue);
		//---VST 3.0.1--------------------------------
		EXPECT_EQ (pis.isPlugInterfaceSupported (IMidiMapping::iid), kResultTrue);
		//---VST 3.1----------------------------------
		EXPECT_EQ (pis.isPlugInterfaceSupported (IEditController2::iid), kResultTrue);
		return true;
	});
	registerTest (TestSuiteName, STR ("Add interface"), [] (ITestResult* testResult) {
		PlugInterfaceSupport pis;
		EXPECT_NE (pis.isPlugInterfaceSupported (IEditControllerHostEditing::iid), kResultTrue);
		pis.addPlugInterfaceSupported (IEditControllerHostEditing::iid);
		EXPECT_EQ (pis.isPlugInterfaceSupported (IEditControllerHostEditing::iid), kResultTrue);
		return true;
	});
	registerTest (TestSuiteName, STR ("Remove interface"), [] (ITestResult* testResult) {
		PlugInterfaceSupport pis;
		EXPECT_NE (pis.isPlugInterfaceSupported (IEditControllerHostEditing::iid), kResultTrue);
		pis.addPlugInterfaceSupported (IEditControllerHostEditing::iid);
		EXPECT_EQ (pis.isPlugInterfaceSupported (IEditControllerHostEditing::iid), kResultTrue);
		EXPECT_TRUE (pis.removePlugInterfaceSupported (IEditControllerHostEditing::iid));
		EXPECT_NE (pis.isPlugInterfaceSupported (IEditControllerHostEditing::iid), kResultTrue);
		return true;
	});
});

//------------------------------------------------------------------------
} // anonymous
} // Vst
} // Steinberg
