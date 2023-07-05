//-----------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/hosting/test/parameterchangestest.cpp
// Created by  : Steinberg, 08/2021
// Description : Test parameter changes
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
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/utility/testing.h"

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {
namespace {

//------------------------------------------------------------------------
struct ValuePoint
{
	int32 sampleOffset {};
	ParamValue value {};
};

//------------------------------------------------------------------------
ModuleInitializer ParameterValueQueueTests ([] () {
	constexpr auto TestSuiteName = "ParameterValueQueue";
	registerTest (TestSuiteName, STR ("Set paramID"), [] (ITestResult* testResult) {
		ParameterValueQueue queue (10);
		EXPECT_EQ (queue.getParameterId (), 10);
		queue.setParamID (5);
		EXPECT_EQ (queue.getParameterId (), 5);
		return true;
	});
	registerTest (TestSuiteName, STR ("Set/get point"), [] (ITestResult* testResult) {
		ParameterValueQueue queue (0);
		ValuePoint vp {100, 0.5};
		int32 index {};
		EXPECT_EQ (queue.addPoint (vp.sampleOffset, vp.value, index), kResultTrue);
		EXPECT_EQ (queue.getPointCount (), 1);
		EXPECT_EQ (index, 0);
		ValuePoint test;
		EXPECT_EQ (queue.getPoint (index, test.sampleOffset, test.value), kResultTrue);
		EXPECT_EQ (vp.sampleOffset, test.sampleOffset);
		EXPECT_EQ (vp.value, test.value);
		return true;
	});
	registerTest (TestSuiteName, STR ("Set/get multiple points"), [] (ITestResult* testResult) {
		ParameterValueQueue queue (0);
		ValuePoint vp1 {10, 0.1};
		ValuePoint vp2 {30, 0.3};
		ValuePoint vp3 {50, 0.6};
		ValuePoint vp4 {70, 0.8};
		int32 index {};
		EXPECT_EQ (queue.addPoint (vp1.sampleOffset, vp1.value, index), kResultTrue);
		EXPECT_EQ (queue.addPoint (vp2.sampleOffset, vp2.value, index), kResultTrue);
		EXPECT_EQ (queue.addPoint (vp3.sampleOffset, vp3.value, index), kResultTrue);
		EXPECT_EQ (queue.addPoint (vp4.sampleOffset, vp4.value, index), kResultTrue);
		EXPECT_EQ (queue.getPointCount (), 4);
		EXPECT_EQ (index, 3);
		ValuePoint test;
		EXPECT_EQ (queue.getPoint (0, test.sampleOffset, test.value), kResultTrue);
		EXPECT_EQ (vp1.sampleOffset, test.sampleOffset);
		EXPECT_EQ (vp1.value, test.value);
		EXPECT_EQ (queue.getPoint (1, test.sampleOffset, test.value), kResultTrue);
		EXPECT_EQ (vp2.sampleOffset, test.sampleOffset);
		EXPECT_EQ (vp2.value, test.value);
		EXPECT_EQ (queue.getPoint (2, test.sampleOffset, test.value), kResultTrue);
		EXPECT_EQ (vp3.sampleOffset, test.sampleOffset);
		EXPECT_EQ (vp3.value, test.value);
		EXPECT_EQ (queue.getPoint (3, test.sampleOffset, test.value), kResultTrue);
		EXPECT_EQ (vp4.sampleOffset, test.sampleOffset);
		EXPECT_EQ (vp4.value, test.value);
		return true;
	});
	registerTest (TestSuiteName, STR ("Ordered points"), [] (ITestResult* testResult) {
		ParameterValueQueue queue (0);
		ValuePoint vp1 {70, 0.1};
		ValuePoint vp2 {50, 0.3};
		ValuePoint vp3 {30, 0.6};
		ValuePoint vp4 {10, 0.8};
		int32 index {};
		EXPECT_EQ (queue.addPoint (vp1.sampleOffset, vp1.value, index), kResultTrue);
		EXPECT_EQ (queue.addPoint (vp2.sampleOffset, vp2.value, index), kResultTrue);
		EXPECT_EQ (queue.addPoint (vp3.sampleOffset, vp3.value, index), kResultTrue);
		EXPECT_EQ (queue.addPoint (vp4.sampleOffset, vp4.value, index), kResultTrue);
		EXPECT_EQ (queue.getPointCount (), 4);
		ValuePoint test;
		EXPECT_EQ (queue.getPoint (0, test.sampleOffset, test.value), kResultTrue);
		EXPECT_EQ (vp4.sampleOffset, test.sampleOffset);
		EXPECT_EQ (vp4.value, test.value);
		EXPECT_EQ (queue.getPoint (1, test.sampleOffset, test.value), kResultTrue);
		EXPECT_EQ (vp3.sampleOffset, test.sampleOffset);
		EXPECT_EQ (vp3.value, test.value);
		EXPECT_EQ (queue.getPoint (2, test.sampleOffset, test.value), kResultTrue);
		EXPECT_EQ (vp2.sampleOffset, test.sampleOffset);
		EXPECT_EQ (vp2.value, test.value);
		EXPECT_EQ (queue.getPoint (3, test.sampleOffset, test.value), kResultTrue);
		EXPECT_EQ (vp1.sampleOffset, test.sampleOffset);
		EXPECT_EQ (vp1.value, test.value);
		return true;
	});
	registerTest (TestSuiteName, STR ("Clear"), [] (ITestResult* testResult) {
		ParameterValueQueue queue (0);
		ValuePoint vp {100, 0.5};
		int32 index {};
		EXPECT_EQ (queue.addPoint (vp.sampleOffset, vp.value, index), kResultTrue);
		EXPECT_EQ (queue.getPointCount (), 1);
		EXPECT_EQ (index, 0);
		queue.clear ();
		EXPECT_EQ (queue.getPointCount (), 0);
		ValuePoint test;
		EXPECT_NE (queue.getPoint (index, test.sampleOffset, test.value), kResultTrue);
		return true;
	});
});

//------------------------------------------------------------------------
ModuleInitializer ParameterChangesTests ([] () {
	constexpr auto TestSuiteName = "ParameterChanges";
	registerTest (TestSuiteName, STR ("Parameter count"), [] (ITestResult* testResult) {
		ParameterChanges changes (1);
		EXPECT_EQ (changes.getParameterCount (), 0);
		int32 index {};
		auto queue = changes.addParameterData (0, index);
		EXPECT_NE (queue, nullptr);
		EXPECT_EQ (index, 0);
		EXPECT_EQ (changes.getParameterCount (), 1);
		return true;
	});
	registerTest (TestSuiteName, STR ("Clear queue"), [] (ITestResult* testResult) {
		ParameterChanges changes (1);
		int32 index {};
		EXPECT_EQ (changes.getParameterCount (), 0);
		changes.addParameterData (0, index);
		EXPECT_EQ (changes.getParameterCount (), 1);
		changes.clearQueue ();
		EXPECT_EQ (changes.getParameterCount (), 0);
		return true;
	});
	registerTest (TestSuiteName, STR ("Increase max parameters"), [] (ITestResult* testResult) {
		ParameterChanges changes (1);
		int32 index {};
		EXPECT_EQ (changes.getParameterCount (), 0);
		changes.addParameterData (0, index);
		EXPECT_EQ (changes.getParameterCount (), 1);
		EXPECT_NE (changes.addParameterData (1, index), nullptr);
		EXPECT_EQ (changes.getParameterCount (), 2);
		changes.setMaxParameters (4);
		EXPECT_EQ (changes.getParameterCount (), 2);
		return true;
	});
	registerTest (TestSuiteName, STR ("Get parameter data"), [] (ITestResult* testResult) {
		ParameterChanges changes (1);
		int32 index {};
		auto queue1 = changes.addParameterData (0, index);
		auto queue2 = changes.getParameterData (index);
		EXPECT_EQ (queue1, queue2);
		return true;
	});
});

//------------------------------------------------------------------------
struct ParamChange
{
	ParamID id {};
	ParamValue value {};
	int32 sampleOffset {};

	bool operator== (const ParamChange& o) const
	{
		return id == o.id && value == o.value && sampleOffset == o.sampleOffset;
	}

	bool operator!= (const ParamChange& o) const
	{
		return id != o.id || value != o.value || sampleOffset != o.sampleOffset;
	}
};

//------------------------------------------------------------------------
ModuleInitializer ParameterChangeTransferTests ([] () {
	constexpr auto TestSuiteName = "ParameterChangeTransfer";
	registerTest (TestSuiteName, STR ("Add/get change"), [] (ITestResult* testResult) {
		ParameterChangeTransfer transfer (1);
		ParamChange change {1, 0.8, 2};
		transfer.addChange (change.id, change.value, change.sampleOffset);
		ParamChange test {};
		EXPECT_NE (change, test);
		EXPECT_TRUE (transfer.getNextChange (test.id, test.value, test.sampleOffset));
		EXPECT_EQ (change, test);
		return true;
	});
	registerTest (TestSuiteName, STR ("Remove changes"), [] (ITestResult* testResult) {
		ParameterChangeTransfer transfer (1);
		ParamChange change {1, 0.8, 2};
		transfer.addChange (change.id, change.value, change.sampleOffset);
		transfer.removeChanges ();
		ParamChange test {};
		EXPECT_FALSE (transfer.getNextChange (test.id, test.value, test.sampleOffset));
		return true;
	});
	registerTest (TestSuiteName, STR ("Transfer changes to"), [] (ITestResult* testResult) {
		ParameterChangeTransfer transfer (10);
		ParamChange ch1 {1, 0.8, 2};
		ParamChange ch2 {2, 0.4, 8};
		transfer.addChange (ch1.id, ch1.value, ch1.sampleOffset);
		transfer.addChange (ch2.id, ch2.value, ch2.sampleOffset);
		ParameterChanges changes (2);
		transfer.transferChangesTo (changes);
		EXPECT_EQ (changes.getParameterCount (), 2);
		auto valueQueue1 = changes.getParameterData (0);
		EXPECT_NE (valueQueue1, nullptr);
		auto valueQueue2 = changes.getParameterData (1);
		EXPECT_NE (valueQueue2, nullptr);
		auto pid1 = valueQueue1->getParameterId ();
		auto pid2 = valueQueue2->getParameterId ();
		EXPECT (pid1 == ch1.id || pid1 == ch2.id);
		EXPECT (pid2 == ch1.id || pid2 == ch2.id);
		EXPECT_NE (pid1, pid2);
		ValuePoint vp1;
		ValuePoint vp2;
		if (pid1 == ch1.id)
		{
			EXPECT_EQ (valueQueue1->getPoint (0, vp1.sampleOffset, vp1.value), kResultTrue);
			EXPECT_EQ (valueQueue2->getPoint (0, vp2.sampleOffset, vp2.value), kResultTrue);
		}
		else
		{
			EXPECT_EQ (valueQueue2->getPoint (0, vp1.sampleOffset, vp1.value), kResultTrue);
			EXPECT_EQ (valueQueue1->getPoint (0, vp2.sampleOffset, vp2.value), kResultTrue);
		}
		return true;
	});
	registerTest (TestSuiteName, STR ("Transfer changes from"), [] (ITestResult* testResult) {
		ParamChange ch1 {1, 0.8, 2};
		ParamChange ch2 {2, 0.4, 8};
		ParameterChangeTransfer transfer (2);
		ParameterChanges changes;
		int32 index {};
		auto valueQueue = changes.addParameterData (ch1.id, index);
		EXPECT_NE (valueQueue, nullptr);
		EXPECT_EQ (valueQueue->addPoint (ch1.sampleOffset, ch1.value, index), kResultTrue);
		valueQueue = changes.addParameterData (ch2.id, index);
		EXPECT_NE (valueQueue, nullptr);
		EXPECT_EQ (valueQueue->addPoint (ch2.sampleOffset, ch2.value, index), kResultTrue);
		transfer.transferChangesFrom (changes);
		ParamChange test1 {};
		ParamChange test2 {};
		ParamChange test3 {};
		EXPECT_TRUE (transfer.getNextChange (test1.id, test1.value, test1.sampleOffset));
		EXPECT_TRUE (transfer.getNextChange (test2.id, test2.value, test2.sampleOffset));
		EXPECT_FALSE (transfer.getNextChange (test3.id, test3.value, test3.sampleOffset));
		EXPECT (test1 == ch1 || test1 == ch2);
		EXPECT (test2 == ch1 || test2 == ch2);
		EXPECT_NE (test1, test2);
		return true;
	});
});

//------------------------------------------------------------------------
} // anonymous
} // Vst
} // Steinberg
