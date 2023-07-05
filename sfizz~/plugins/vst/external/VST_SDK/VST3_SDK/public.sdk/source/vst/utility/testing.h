//------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : VST SDK
//
// Category    : Examples
// Filename    : public.sdk/source/vst/utility/testing.h
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

#pragma once

#include "pluginterfaces/test/itest.h"
#include <cmath>
#include <functional>
#include <limits>

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
/**

\page How to use the validator to run your own tests?

It is possible to run your own tests when the validator checks your plug-in.

First you have to register a test factory in your plugin factory:

\code{.cpp}

#include "public.sdk/source/vst/utility/testing.h"

BEGIN_FACTORY_DEF(...

DEF_CLASS2 (Your Plugin Processor)
DEF_CLASS2 (Your Plugin Controller)

DEF_CLASS2 (INLINE_UID_FROM_FUID (getTestFactoryUID ()), PClassInfo::kManyInstances, kTestClass,
           "Test Factory", 0, "", "", "", createTestFactoryInstance)

END_FACTORY

\endcode

Second: write your tests:

\code{.cpp}

#include "public.sdk/source/main/moduleinit.h"
#include "public.sdk/source/vst/utility/testing.h"

static ModuleInitializer InitMyTests ([] () {
    registerTest ("MyTests", STR ("two plus two is four"), [] (ITestResult* testResult) {
        auto result = 2 + 2;
        if (result == 4)
            return true;
        testResult->addErrorMessage (STR ("Unexpected universe change where 2+2 != 4."));
        return false;
    });
});

\endcode

If you need access to your audio effect or edit controller you can write your tests as done in the
adelay example:

\code{.cpp}

#include "public.sdk/source/main/moduleinit.h"
#include "public.sdk/source/vst/testsuite/vsttestsuite.h"
#include "public.sdk/source/vst/utility/testing.h"

static ModuleInitializer InitMyTests ([] () {
    registerTest ("MyTests", STR ("check one two three"), [] (FUnknown* context, ITestResult*
                                                              testResult)
    {
        FUnknownPtr<ITestPlugProvider> plugProvider (context);
        if (plugProvider)
        {
            auto controller = plugProvider->getController ();
            FUnknownPtr<IDelayTestController> testController (controller);
            if (!controller)
            {
                testResult->addErrorMessage (String ("Unknown IEditController"));
                return false;
            }
            bool result = testController->doTest ();
            plugProvider->releasePlugIn (nullptr, controller);

            return (result);
        }
        return false;
    });
});

\endcode

After that recompile and if the validator does not run automatically after every build, start the
validator manually and let it check your plug-in.

*/

//------------------------------------------------------------------------
/** create a Test Factory instance */
FUnknown* createTestFactoryInstance (void*);

/** the test factory class ID */
static const DECLARE_UID (TestFactoryUID, 0x70AA33A3, 0x1AE74B24, 0xB726F784, 0xB706C080);

/** get the test factory class ID */
const FUID& getTestFactoryUID ();

/** simple test function */
using TestFunc = std::function<bool (ITestResult*)>;
/** register a simple test function */
void registerTest (FIDString name, const tchar* desc, const TestFunc& func);
/** register a simple test function */
void registerTest (FIDString name, const tchar* desc, TestFunc&& func);

/** test function with context pointer */
using TestFuncWithContext = std::function<bool (FUnknown*, ITestResult*)>;
/** register a test function with context pointer */
void registerTest (FIDString name, const tchar* desc, const TestFuncWithContext& func);
/** register a test function with context pointer */
void registerTest (FIDString name, const tchar* desc, TestFuncWithContext&& func);

/** register a custom test, the test object will be owned by the implementation */
void registerTest (FIDString name, ITest* test);

//------------------------------------------------------------------------
namespace Test {

//------------------------------------------------------------------------
template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
inline constexpr bool equal (const T& lhs, const T& rhs) noexcept
{
	return std::abs (lhs - rhs) <= std::numeric_limits<T>::epsilon ();
}

//------------------------------------------------------------------------
template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
inline constexpr bool equal (const T& lhs, const T& rhs) noexcept
{
	return lhs == rhs;
}

//------------------------------------------------------------------------
template <typename T>
inline constexpr bool notEqual (const T& lhs, const T& rhs) noexcept
{
	return equal (lhs, rhs) == false;
}

//------------------------------------------------------------------------
template <typename T>
inline constexpr bool maxDiff (const T& lhs, const T& rhs, const T& maxDiff) noexcept
{
	return std::abs (lhs - rhs) <= maxDiff;
}

#ifndef SMTG_DISABLE_VST_TEST_MACROS

#ifndef SMTG_MAKE_STRING_PRIVATE_DONT_USE
#define SMTG_MAKE_STRING_PRIVATE_DONT_USE(x) #x
#define SMTG_MAKE_STRING(x) SMTG_MAKE_STRING_PRIVATE_DONT_USE (x)
#endif // SMTG_MAKE_STRING_PRIVATE_DONT_USE

#define EXPECT(condition)                                                     \
	{                                                                         \
		if (!(condition))                                                     \
		{                                                                     \
			testResult->addErrorMessage (STR (__FILE__ ":" SMTG_MAKE_STRING ( \
			    __LINE__) ": error: " SMTG_MAKE_STRING (condition)));         \
			return false;                                                     \
		}                                                                     \
	}

#define EXPECT_TRUE(condition) EXPECT (condition)
#define EXPECT_FALSE(condition) EXPECT (!condition)
#define EXPECT_EQ(var1, var2) EXPECT ((var1 == var2))
#define EXPECT_NE(var1, var2) EXPECT ((var1 != var2))

#endif // SMTG_DISABLE_VST_TEST_MACROS

//------------------------------------------------------------------------
} // Test
} // Vst
} // Steinberg
