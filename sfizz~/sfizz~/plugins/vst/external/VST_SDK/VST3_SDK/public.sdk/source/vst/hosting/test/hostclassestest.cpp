//-----------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/hosting/test/hostclassestest.cpp
// Created by  : Steinberg, 08/2021
// Description : Test host classes
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
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/utility/testing.h"
#include "pluginterfaces/base/fstrdefs.h"

#include <array>
#include <vector>

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {
namespace {

//------------------------------------------------------------------------
ModuleInitializer HostApplicationTests ([] () {
	constexpr auto TestSuiteName = "HostApplication";
	registerTest (
	    TestSuiteName, STR ("Create instance of IAttributeList"), [] (ITestResult* testResult) {
		    HostApplication hostApp;
		    FUnknown* instance {nullptr};
		    TUID iid;
		    IAttributeList::iid.toTUID (iid);
		    EXPECT_EQ (hostApp.createInstance (iid, iid, reinterpret_cast<void**> (&instance)),
		               kResultTrue);
		    EXPECT_NE (instance, nullptr);
		    instance->release ();
		    return true;
	    });
	registerTest (TestSuiteName, STR ("Create instance of IMessage"), [] (ITestResult* testResult) {
		HostApplication hostApp;
		FUnknown* instance {nullptr};
		TUID iid;
		IMessage::iid.toTUID (iid);
		EXPECT_EQ (hostApp.createInstance (iid, iid, reinterpret_cast<void**> (&instance)),
		           kResultTrue);
		EXPECT_NE (instance, nullptr);
		instance->release ();
		return true;
	});
});

//------------------------------------------------------------------------
ModuleInitializer HostAttributeListTests ([] () {
	constexpr auto TestSuiteName = "HostAttributeList";
	registerTest (TestSuiteName, STR ("Int"), [] (ITestResult* testResult) {
		auto attrList = HostAttributeList::make ();
		constexpr int64 testValue = 5;
		EXPECT_EQ (attrList->setInt ("Int", testValue), kResultTrue);
		int64 value = 0;
		EXPECT_EQ (attrList->getInt ("Int", value), kResultTrue);
		EXPECT_EQ (value, testValue);
		return true;
	});
	registerTest (TestSuiteName, STR ("Float"), [] (ITestResult* testResult) {
		auto attrList = HostAttributeList::make ();
		constexpr double testValue = 2.636;
		EXPECT_EQ (attrList->setFloat ("Float", testValue), kResultTrue);
		double value = 0;
		EXPECT_EQ (attrList->getFloat ("Float", value), kResultTrue);
		EXPECT_EQ (value, testValue);
		return true;
	});
	registerTest (TestSuiteName, STR ("String"), [] (ITestResult* testResult) {
		auto attrList = HostAttributeList::make ();
		constexpr const TChar* testValue = STR ("TestValue");
		EXPECT_EQ (attrList->setString ("Str", testValue), kResultTrue);
		TChar value[10];
		EXPECT_EQ (attrList->getString ("Str", value, 10 * sizeof (TChar)), kResultTrue);
		EXPECT_EQ (tstrcmp (testValue, value), 0);
		return true;
	});
	registerTest (TestSuiteName, STR ("Binary"), [] (ITestResult* testResult) {
		auto attrList = HostAttributeList::make ();
		std::array<int32, 20> testData;
		uint32 testDataSize = static_cast<uint32>(testData.size ()) * sizeof (int32);
		EXPECT_EQ (attrList->setBinary ("Binary", testData.data (), testDataSize), kResultTrue);
		const void* data;
		uint32 dataSize {0};
		EXPECT_EQ (attrList->getBinary ("Binary", data, dataSize), kResultTrue);
		EXPECT_EQ (dataSize, testDataSize);
		auto s = reinterpret_cast<const int32*> (data);
		for (auto i : testData)
		{
			EXPECT_EQ (i, *s);
			s++;
		}
		return true;
	});
	registerTest (TestSuiteName, STR ("Multiple Set"), [] (ITestResult* testResult) {
		auto attrList = HostAttributeList::make ();
		constexpr int64 testValue1 = 5;
		constexpr int64 testValue2 = 6;
		constexpr int64 testValue3 = 7;
		EXPECT_EQ (attrList->setInt ("Int", testValue1), kResultTrue);
		EXPECT_EQ (attrList->setInt ("Int", testValue2), kResultTrue);
		EXPECT_EQ (attrList->setInt ("Int", testValue3), kResultTrue);
		int64 value = 0;
		EXPECT_EQ (attrList->getInt ("Int", value), kResultTrue);
		EXPECT_EQ (value, testValue3);
		return true;
	});
});

//------------------------------------------------------------------------
} // anonymous
} // Vst
} // Steinberg
