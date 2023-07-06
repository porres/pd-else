//------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : VST SDK
//
// Category    : Examples
// Filename    : public.sdk/source/vst/utility/testing.cpp
// Created by  : Steinberg, 04/2021
// Description : Utility classes for custom testing in the vst validator
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

#include "public.sdk/source/vst/utility/testing.h"
#include <atomic>
#include <cassert>
#include <string>
#include <vector>

//------------------------------------------------------------------------
namespace Steinberg {

DEF_CLASS_IID (ITest)
DEF_CLASS_IID (ITestSuite)
DEF_CLASS_IID (ITestFactory)

//------------------------------------------------------------------------
namespace Vst {
namespace {

//------------------------------------------------------------------------
struct TestRegistry
{
	struct TestWithContext
	{
		std::u16string desc;
		TestFuncWithContext func;
	};
	using Tests = std::vector<std::pair<std::string, IPtr<ITest>>>;
	using TestsWithContext = std::vector<std::pair<std::string, TestWithContext>>;

	static TestRegistry& instance ()
	{
		static TestRegistry gInstance;
		return gInstance;
	}

	Tests tests;
	TestsWithContext testsWithContext;
};

//------------------------------------------------------------------------
struct TestBase : ITest
{
	TestBase (const tchar* inDesc)
	{
		if (inDesc)
			desc = reinterpret_cast<const std::u16string::value_type*> (inDesc);
	}
	TestBase (const std::u16string& inDesc) : desc (inDesc) {}

	virtual ~TestBase () = default;

	bool PLUGIN_API setup () override { return true; }
	bool PLUGIN_API teardown () override { return true; }
	const tchar* PLUGIN_API getDescription () override
	{
		return reinterpret_cast<const tchar*> (desc.data ());
	}

	tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) override
	{
		QUERY_INTERFACE (_iid, obj, FUnknown::iid, FUnknown)
		QUERY_INTERFACE (_iid, obj, ITest::iid, ITest)
		*obj = nullptr;
		return kNoInterface;
	}
	uint32 PLUGIN_API addRef () override { return ++refCount; }
	uint32 PLUGIN_API release () override
	{
		if (--refCount == 0)
		{
			delete this;
			return 0;
		}
		return refCount;
	}

	std::atomic<uint32> refCount {1};
	std::u16string desc;
};

//------------------------------------------------------------------------
struct FuncTest : TestBase
{
	FuncTest (const tchar* desc, const TestFunc& func) : TestBase (desc), func (func) {}
	FuncTest (const tchar* desc, TestFunc&& func) : TestBase (desc), func (std::move (func)) {}

	bool PLUGIN_API run (ITestResult* testResult) override { return func (testResult); }

	TestFunc func;
};

//------------------------------------------------------------------------
struct FuncWithContextTest : TestBase
{
	FuncWithContextTest (FUnknown* context, const std::u16string& desc,
	                     const TestFuncWithContext& func)
	: TestBase (desc), func (func), context (context)
	{
	}

	bool PLUGIN_API run (ITestResult* testResult) override { return func (context, testResult); }

	TestFuncWithContext func;
	FUnknown* context;
};

//------------------------------------------------------------------------
struct TestFactoryImpl : ITestFactory
{
	TestFactoryImpl () = default;
	virtual ~TestFactoryImpl () = default;

	tresult PLUGIN_API createTests (FUnknown* context, ITestSuite* parentSuite) override
	{
		for (auto& t : TestRegistry::instance ().tests)
			parentSuite->addTest (t.first.data (), t.second);
		for (auto& t : TestRegistry::instance ().testsWithContext)
			parentSuite->addTest (t.first.data (),
			                      new FuncWithContextTest (context, t.second.desc, t.second.func));
		return kResultTrue;
	}
	tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) override
	{
		QUERY_INTERFACE (_iid, obj, FUnknown::iid, FUnknown)
		QUERY_INTERFACE (_iid, obj, ITestFactory::iid, ITestFactory)
		*obj = nullptr;
		return kNoInterface;
	}

	uint32 PLUGIN_API addRef () override { return ++refCount; }

	uint32 PLUGIN_API release () override
	{
		if (--refCount == 0)
		{
			delete this;
			return 0;
		}
		return refCount;
	}

private:
	std::atomic<uint32> refCount {1};
};

//------------------------------------------------------------------------
} // anonymous

//------------------------------------------------------------------------
void registerTest (FIDString name, const tchar* desc, const TestFunc& func)
{
	registerTest (name, new FuncTest (desc, func));
}

//------------------------------------------------------------------------
void registerTest (FIDString name, const tchar* desc, TestFunc&& func)
{
	registerTest (name, new FuncTest (desc, std::move (func)));
}

//------------------------------------------------------------------------
void registerTest (FIDString name, ITest* test)
{
	assert (name != nullptr);
	TestRegistry::instance ().tests.push_back (std::make_pair (name, owned (test)));
}

//------------------------------------------------------------------------
void registerTest (FIDString name, const tchar* desc, const TestFuncWithContext& func)
{
	std::u16string descStr;
	if (desc)
		descStr = reinterpret_cast<const std::u16string::value_type*> (desc);
	TestRegistry::instance ().testsWithContext.push_back (
	    std::make_pair (name, TestRegistry::TestWithContext {descStr, func}));
}

//------------------------------------------------------------------------
void registerTest (FIDString name, const tchar* desc, TestFuncWithContext&& func)
{
	std::u16string descStr;
	if (desc)
		descStr = reinterpret_cast<const std::u16string::value_type*> (desc);
	TestRegistry::instance ().testsWithContext.push_back (
	    std::make_pair (name, TestRegistry::TestWithContext {descStr, std::move (func)}));
}

//------------------------------------------------------------------------
FUnknown* createTestFactoryInstance (void*)
{
	return new TestFactoryImpl;
}

//------------------------------------------------------------------------
const FUID& getTestFactoryUID ()
{
	static FUID uid = FUID::fromTUID (TestFactoryUID);
	return uid;
}

//------------------------------------------------------------------------
} // Vst
} // Steinberg
