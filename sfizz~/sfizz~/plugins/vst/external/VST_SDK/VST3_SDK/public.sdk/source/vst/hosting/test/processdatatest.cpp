//-----------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/hosting/test/processdatatest.cpp
// Created by  : Steinberg, 08/2021
// Description : Test process data helper
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
#include "public.sdk/source/vst/hosting/processdata.h"
#include "public.sdk/source/vst/utility/testing.h"

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstunits.h"

#include <functional>
#include <memory>

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {
namespace {

//------------------------------------------------------------------------
struct TestComponent : public IComponent
{
	using GetBusCountFunc = std::function<int32 (BusDirection dir)>;
	using GetBusInfoFunc = std::function<tresult (BusDirection dir, int32 index, BusInfo& bus)>;

	tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) override
	{
		return kNoInterface;
	}
	uint32 PLUGIN_API addRef () override { return 100; }
	uint32 PLUGIN_API release () override { return 100; }
	tresult PLUGIN_API initialize (FUnknown* context) override { return kResultTrue; }
	tresult PLUGIN_API terminate () override { return kResultTrue; }
	tresult PLUGIN_API getControllerClassId (TUID classId) override { return kNotImplemented; }
	tresult PLUGIN_API setIoMode (IoMode mode) override { return kNotImplemented; }
	int32 PLUGIN_API getBusCount (MediaType type, BusDirection dir) override
	{
		if (type != MediaTypes::kAudio)
			return 0;
		return getBusCountFunc (dir);
	}
	tresult PLUGIN_API getBusInfo (MediaType type, BusDirection dir, int32 index,
	                               BusInfo& bus) override
	{
		if (type != MediaTypes::kAudio)
			return kResultFalse;
		return getBusInfoFunc (dir, index, bus);
	}
	tresult PLUGIN_API getRoutingInfo (RoutingInfo& inInfo, RoutingInfo& outInfo) override
	{
		return kNotImplemented;
	}
	tresult PLUGIN_API activateBus (MediaType type, BusDirection dir, int32 index,
	                                TBool state) override
	{
		return kNotImplemented;
	}
	tresult PLUGIN_API setActive (TBool state) override { return kNotImplemented; }
	tresult PLUGIN_API setState (IBStream* state) override { return kNotImplemented; }
	tresult PLUGIN_API getState (IBStream* state) override { return kNotImplemented; }

	GetBusCountFunc getBusCountFunc = [] (BusDirection dir) { return 0; };
	GetBusInfoFunc getBusInfoFunc = [] (BusDirection dir, int32 index, BusInfo& bus) {
		return kNotImplemented;
	};
};

//------------------------------------------------------------------------
ModuleInitializer HostProcessDataTests ([] () {
	constexpr auto TestSuiteName = "HostProcessData";
	registerTest (TestSuiteName, STR ("No bus"), [] (ITestResult* testResult) {
		TestComponent tc;
		HostProcessData processData;
		EXPECT_TRUE (processData.prepare (tc, 1024, kSample32));
		EXPECT_EQ (processData.numInputs, 0);
		EXPECT_EQ (processData.numOutputs, 0);
		return true;
	});
	registerTest (TestSuiteName, STR ("1 out bus no channels"), [] (ITestResult* testResult) {
		TestComponent tc;
		tc.getBusCountFunc = [] (BusDirection dir) {
			return dir == BusDirections::kOutput ? 1 : 0;
		};
		HostProcessData processData;
		EXPECT_TRUE (processData.prepare (tc, 1024, kSample32));
		EXPECT_EQ (processData.numInputs, 0);
		EXPECT_EQ (processData.numOutputs, 1);
		EXPECT_EQ (processData.outputs[0].numChannels, 0);
		return true;
	});
	registerTest (TestSuiteName, STR ("1 out bus 2 channels"), [] (ITestResult* testResult) {
		TestComponent tc;
		tc.getBusCountFunc = [] (BusDirection dir) {
			return dir == BusDirections::kOutput ? 1 : 0;
		};
		tc.getBusInfoFunc = [] (BusDirection dir, int32 index, BusInfo& bus) {
			if (dir == BusDirections::kInput || index != 0)
				return kResultFalse;
			bus.channelCount = 2;
			return kResultTrue;
		};
		HostProcessData processData;
		EXPECT_TRUE (processData.prepare (tc, 1024, kSample32));
		EXPECT_EQ (processData.numInputs, 0);
		EXPECT_EQ (processData.numOutputs, 1);
		EXPECT_EQ (processData.outputs[0].numChannels, 2);
		EXPECT_NE (processData.outputs[0].channelBuffers32[0], nullptr);
		EXPECT_NE (processData.outputs[0].channelBuffers32[1], nullptr);
		return true;
	});
	registerTest (TestSuiteName, STR ("1 in & out bus 2 channels"), [] (ITestResult* testResult) {
		TestComponent tc;
		tc.getBusCountFunc = [] (BusDirection dir) { return 1; };
		tc.getBusInfoFunc = [] (BusDirection dir, int32 index, BusInfo& bus) {
			if (index != 0)
				return kResultFalse;
			bus.channelCount = 2;
			return kResultTrue;
		};
		HostProcessData processData;
		EXPECT_TRUE (processData.prepare (tc, 1024, kSample32));
		EXPECT_EQ (processData.numOutputs, 1);
		EXPECT_EQ (processData.outputs[0].numChannels, 2);
		EXPECT_NE (processData.outputs[0].channelBuffers32[0], nullptr);
		EXPECT_NE (processData.outputs[0].channelBuffers32[1], nullptr);
		EXPECT_EQ (processData.numInputs, 1);
		EXPECT_EQ (processData.inputs[0].numChannels, 2);
		EXPECT_NE (processData.inputs[0].channelBuffers32[0], nullptr);
		EXPECT_NE (processData.inputs[0].channelBuffers32[1], nullptr);
		return true;
	});
	registerTest (TestSuiteName, STR ("2 in & out bus dif channels"), [] (ITestResult* testResult) {
		TestComponent tc;
		tc.getBusCountFunc = [] (BusDirection dir) { return 2; };
		tc.getBusInfoFunc = [] (BusDirection dir, int32 index, BusInfo& bus) {
			if (index < 0 || index > 1)
				return kResultFalse;
			bus.channelCount = index == 0 ? 4 : 1;
			return kResultTrue;
		};
		HostProcessData processData;
		EXPECT_TRUE (processData.prepare (tc, 1024, kSample32));
		EXPECT_EQ (processData.numOutputs, 2);
		EXPECT_EQ (processData.outputs[0].numChannels, 4);
		EXPECT_NE (processData.outputs[0].channelBuffers32[0], nullptr);
		EXPECT_NE (processData.outputs[0].channelBuffers32[1], nullptr);
		EXPECT_NE (processData.outputs[0].channelBuffers32[2], nullptr);
		EXPECT_NE (processData.outputs[0].channelBuffers32[3], nullptr);
		EXPECT_EQ (processData.outputs[1].numChannels, 1);
		EXPECT_NE (processData.outputs[1].channelBuffers32[0], nullptr);
		EXPECT_EQ (processData.numInputs, 2);
		EXPECT_EQ (processData.inputs[0].numChannels, 4);
		EXPECT_NE (processData.inputs[0].channelBuffers32[0], nullptr);
		EXPECT_NE (processData.inputs[0].channelBuffers32[1], nullptr);
		EXPECT_NE (processData.inputs[0].channelBuffers32[2], nullptr);
		EXPECT_NE (processData.inputs[0].channelBuffers32[3], nullptr);
		EXPECT_EQ (processData.inputs[1].numChannels, 1);
		EXPECT_NE (processData.inputs[1].channelBuffers32[0], nullptr);
		return true;
	});
	registerTest (TestSuiteName, STR ("Set all channel buffers 32"), [] (ITestResult* testResult) {
		TestComponent tc;
		tc.getBusCountFunc = [] (BusDirection dir) { return 1; };
		tc.getBusInfoFunc = [] (BusDirection dir, int32 index, BusInfo& bus) {
			if (index != 0)
				return kResultFalse;
			bus.channelCount = 2;
			return kResultTrue;
		};
		auto buffer = std::unique_ptr<float[]> (new float[10]);
		HostProcessData processData;
		EXPECT_TRUE (processData.prepare (tc, 0, kSample32));
		EXPECT_EQ (processData.numInputs, 1);
		EXPECT_EQ (processData.numOutputs, 1);
		EXPECT_FALSE (processData.setChannelBuffers (BusDirections::kInput, 1, nullptr));
		EXPECT_FALSE (processData.setChannelBuffers64 (BusDirections::kInput, 1, nullptr));
		EXPECT_TRUE (processData.setChannelBuffers (BusDirections::kInput, 0, buffer.get ()));
		EXPECT_TRUE (processData.setChannelBuffers (BusDirections::kOutput, 0, buffer.get ()));
		EXPECT_FALSE (processData.setChannelBuffers64 (BusDirections::kInput, 0, nullptr));
		EXPECT_FALSE (processData.setChannelBuffers64 (BusDirections::kOutput, 0, nullptr));
		EXPECT_EQ (processData.inputs[0].channelBuffers32[0], buffer.get ());
		EXPECT_EQ (processData.inputs[0].channelBuffers32[1], buffer.get ());
		EXPECT_EQ (processData.outputs[0].channelBuffers32[0], buffer.get ());
		EXPECT_EQ (processData.outputs[0].channelBuffers32[1], buffer.get ());
		return true;
	});
	registerTest (TestSuiteName, STR ("Set all channel buffers 64"), [] (ITestResult* testResult) {
		TestComponent tc;
		tc.getBusCountFunc = [] (BusDirection dir) { return 1; };
		tc.getBusInfoFunc = [] (BusDirection dir, int32 index, BusInfo& bus) {
			if (index != 0)
				return kResultFalse;
			bus.channelCount = 2;
			return kResultTrue;
		};
		auto buffer = std::unique_ptr<double[]> (new double[10]);
		HostProcessData processData;
		EXPECT_TRUE (processData.prepare (tc, 0, kSample64));
		EXPECT_EQ (processData.numInputs, 1);
		EXPECT_EQ (processData.numOutputs, 1);
		EXPECT_FALSE (processData.setChannelBuffers (BusDirections::kInput, 1, nullptr));
		EXPECT_FALSE (processData.setChannelBuffers64 (BusDirections::kInput, 1, nullptr));
		EXPECT_TRUE (processData.setChannelBuffers64 (BusDirections::kInput, 0, buffer.get ()));
		EXPECT_TRUE (processData.setChannelBuffers64 (BusDirections::kOutput, 0, buffer.get ()));
		EXPECT_FALSE (processData.setChannelBuffers (BusDirections::kInput, 0, nullptr));
		EXPECT_FALSE (processData.setChannelBuffers (BusDirections::kOutput, 0, nullptr));
		EXPECT_EQ (processData.inputs[0].channelBuffers64[0], buffer.get ());
		EXPECT_EQ (processData.inputs[0].channelBuffers64[1], buffer.get ());
		EXPECT_EQ (processData.outputs[0].channelBuffers64[0], buffer.get ());
		EXPECT_EQ (processData.outputs[0].channelBuffers64[1], buffer.get ());
		return true;
	});
	registerTest (
	    TestSuiteName, STR ("Set individual channel buffers 32"), [] (ITestResult* testResult) {
		    TestComponent tc;
		    tc.getBusCountFunc = [] (BusDirection dir) { return 1; };
		    tc.getBusInfoFunc = [] (BusDirection dir, int32 index, BusInfo& bus) {
			    if (index != 0)
				    return kResultFalse;
			    bus.channelCount = 2;
			    return kResultTrue;
		    };
		    auto bufferL = std::unique_ptr<float[]> (new float[10]);
		    auto bufferR = std::unique_ptr<float[]> (new float[10]);
		    HostProcessData pd;
		    EXPECT_TRUE (pd.prepare (tc, 0, kSample32));
		    EXPECT_EQ (pd.numInputs, 1);
		    EXPECT_EQ (pd.numOutputs, 1);
		    EXPECT_FALSE (pd.setChannelBuffer (BusDirections::kInput, 1, 0, nullptr));
		    EXPECT_FALSE (pd.setChannelBuffer64 (BusDirections::kInput, 1, 0, nullptr));
		    EXPECT_TRUE (pd.setChannelBuffer (BusDirections::kInput, 0, 0, bufferL.get ()));
		    EXPECT_TRUE (pd.setChannelBuffer (BusDirections::kInput, 0, 1, bufferR.get ()));
		    EXPECT_TRUE (pd.setChannelBuffer (BusDirections::kOutput, 0, 0, bufferL.get ()));
		    EXPECT_TRUE (pd.setChannelBuffer (BusDirections::kOutput, 0, 1, bufferR.get ()));
		    EXPECT_FALSE (pd.setChannelBuffer64 (BusDirections::kInput, 0, 0, nullptr));
		    EXPECT_FALSE (pd.setChannelBuffer64 (BusDirections::kOutput, 0, 1, nullptr));
		    EXPECT_EQ (pd.inputs[0].channelBuffers32[0], bufferL.get ());
		    EXPECT_EQ (pd.inputs[0].channelBuffers32[1], bufferR.get ());
		    EXPECT_EQ (pd.outputs[0].channelBuffers32[0], bufferL.get ());
		    EXPECT_EQ (pd.outputs[0].channelBuffers32[1], bufferR.get ());
		    return true;
	    });
	registerTest (TestSuiteName, STR ("Set individual channel buffers 32 combined"),
	              [] (ITestResult* testResult) {
		              TestComponent tc;
		              tc.getBusCountFunc = [] (BusDirection dir) { return 1; };
		              tc.getBusInfoFunc = [] (BusDirection dir, int32 index, BusInfo& bus) {
			              if (index != 0)
				              return kResultFalse;
			              bus.channelCount = 2;
			              return kResultTrue;
		              };
		              auto bufferL = std::unique_ptr<float[]> (new float[10]);
		              auto bufferR = std::unique_ptr<float[]> (new float[10]);
		              float* buffers[2] = {bufferL.get (), bufferR.get ()};
		              HostProcessData pd;
		              EXPECT_TRUE (pd.prepare (tc, 0, kSample32));
		              EXPECT_EQ (pd.numInputs, 1);
		              EXPECT_EQ (pd.numOutputs, 1);
		              EXPECT_FALSE (pd.setChannelBuffers (BusDirections::kInput, 1, nullptr, 0));
		              EXPECT_FALSE (pd.setChannelBuffers64 (BusDirections::kInput, 1, nullptr, 0));
		              EXPECT_TRUE (pd.setChannelBuffers (BusDirections::kInput, 0, buffers, 2));
		              EXPECT_TRUE (pd.setChannelBuffers (BusDirections::kOutput, 0, buffers, 2));
		              EXPECT_FALSE (pd.setChannelBuffers64 (BusDirections::kInput, 0, nullptr, 0));
		              EXPECT_FALSE (pd.setChannelBuffers64 (BusDirections::kOutput, 0, nullptr, 0));
		              EXPECT_EQ (pd.inputs[0].channelBuffers32[0], bufferL.get ());
		              EXPECT_EQ (pd.inputs[0].channelBuffers32[1], bufferR.get ());
		              EXPECT_EQ (pd.outputs[0].channelBuffers32[0], bufferL.get ());
		              EXPECT_EQ (pd.outputs[0].channelBuffers32[1], bufferR.get ());
		              return true;
	              });
	registerTest (
	    TestSuiteName, STR ("Set individual channel buffers 64"), [] (ITestResult* testResult) {
		    TestComponent tc;
		    tc.getBusCountFunc = [] (BusDirection dir) { return 1; };
		    tc.getBusInfoFunc = [] (BusDirection dir, int32 index, BusInfo& bus) {
			    if (index != 0)
				    return kResultFalse;
			    bus.channelCount = 2;
			    return kResultTrue;
		    };
		    auto bufferL = std::unique_ptr<double[]> (new double[10]);
		    auto bufferR = std::unique_ptr<double[]> (new double[10]);
		    HostProcessData pd;
		    EXPECT_TRUE (pd.prepare (tc, 0, kSample64));
		    EXPECT_EQ (pd.numInputs, 1);
		    EXPECT_EQ (pd.numOutputs, 1);
		    EXPECT_FALSE (pd.setChannelBuffer (BusDirections::kInput, 1, 0, nullptr));
		    EXPECT_FALSE (pd.setChannelBuffer64 (BusDirections::kInput, 1, 0, nullptr));
		    EXPECT_TRUE (pd.setChannelBuffer64 (BusDirections::kInput, 0, 0, bufferL.get ()));
		    EXPECT_TRUE (pd.setChannelBuffer64 (BusDirections::kInput, 0, 1, bufferR.get ()));
		    EXPECT_TRUE (pd.setChannelBuffer64 (BusDirections::kOutput, 0, 0, bufferL.get ()));
		    EXPECT_TRUE (pd.setChannelBuffer64 (BusDirections::kOutput, 0, 1, bufferR.get ()));
		    EXPECT_FALSE (pd.setChannelBuffer (BusDirections::kInput, 0, 0, nullptr));
		    EXPECT_FALSE (pd.setChannelBuffer (BusDirections::kOutput, 0, 1, nullptr));
		    EXPECT_EQ (pd.inputs[0].channelBuffers64[0], bufferL.get ());
		    EXPECT_EQ (pd.inputs[0].channelBuffers64[1], bufferR.get ());
		    EXPECT_EQ (pd.outputs[0].channelBuffers64[0], bufferL.get ());
		    EXPECT_EQ (pd.outputs[0].channelBuffers64[1], bufferR.get ());
		    return true;
	    });
	registerTest (TestSuiteName, STR ("Set individual channel buffers 64 combined"),
	              [] (ITestResult* testResult) {
		              TestComponent tc;
		              tc.getBusCountFunc = [] (BusDirection dir) { return 1; };
		              tc.getBusInfoFunc = [] (BusDirection dir, int32 index, BusInfo& bus) {
			              if (index != 0)
				              return kResultFalse;
			              bus.channelCount = 2;
			              return kResultTrue;
		              };
		              auto bufferL = std::unique_ptr<double[]> (new double[10]);
		              auto bufferR = std::unique_ptr<double[]> (new double[10]);
		              double* buffers[2] = {bufferL.get (), bufferR.get ()};
		              HostProcessData pd;
		              EXPECT_TRUE (pd.prepare (tc, 0, kSample64));
		              EXPECT_EQ (pd.numInputs, 1);
		              EXPECT_EQ (pd.numOutputs, 1);
		              EXPECT_FALSE (pd.setChannelBuffers (BusDirections::kInput, 1, nullptr, 0));
		              EXPECT_FALSE (pd.setChannelBuffers64 (BusDirections::kInput, 1, nullptr, 0));
		              EXPECT_TRUE (pd.setChannelBuffers64 (BusDirections::kInput, 0, buffers, 2));
		              EXPECT_TRUE (pd.setChannelBuffers64 (BusDirections::kOutput, 0, buffers, 2));
		              EXPECT_FALSE (pd.setChannelBuffers (BusDirections::kInput, 0, nullptr, 0));
		              EXPECT_FALSE (pd.setChannelBuffers (BusDirections::kOutput, 0, nullptr, 0));
		              EXPECT_EQ (pd.inputs[0].channelBuffers64[0], bufferL.get ());
		              EXPECT_EQ (pd.inputs[0].channelBuffers64[1], bufferR.get ());
		              EXPECT_EQ (pd.outputs[0].channelBuffers64[0], bufferL.get ());
		              EXPECT_EQ (pd.outputs[0].channelBuffers64[1], bufferR.get ());
		              return true;
	              });
});

//------------------------------------------------------------------------
} // anonymous
} // Vst
} // Steinberg
