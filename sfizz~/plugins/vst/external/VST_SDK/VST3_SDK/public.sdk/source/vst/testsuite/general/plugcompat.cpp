//-----------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : VST SDK
//
// Category    : Validator
// Filename    : public.sdk/source/vst/testsuite/general/plugcompat.cpp
// Created by  : Steinberg, 03/2022
// Description : VST Test Suite
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
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#include "public.sdk/source/vst/testsuite/general/plugcompat.h"
#include "public.sdk/source/vst/moduleinfo/moduleinfoparser.h"
#include "pluginterfaces/base/funknownimpl.h"
#include <string>

//------------------------------------------------------------------------
namespace Steinberg {

//------------------------------------------------------------------------
namespace {

//------------------------------------------------------------------------
struct StringStream : U::ImplementsNonDestroyable<U::Directly<IBStream>>
{
	std::string str;
	tresult PLUGIN_API read (void*, int32, int32*) override { return kNotImplemented; }
	tresult PLUGIN_API write (void* buffer, int32 numBytes, int32* numBytesWritten) override
	{
		str.append (static_cast<char*> (buffer), numBytes);
		if (numBytesWritten)
			*numBytesWritten = numBytes;
		return kResultTrue;
	}
	tresult PLUGIN_API seek (int64, int32, int64*) override { return kNotImplemented; }
	tresult PLUGIN_API tell (int64* pos) override { return kNotImplemented; }
};

//------------------------------------------------------------------------
} // anonymous

//------------------------------------------------------------------------
bool checkPluginCompatibility (VST3::Hosting::Module::Ptr& module,
                               IPtr<IPluginCompatibility> compat, std::ostream* errorStream)
{
	bool failure = false;
	if (auto moduleInfoPath = VST3::Hosting::Module::getModuleInfoPath (module->getPath ()))
	{
		failure = true;
		if (errorStream)
		{
			*errorStream
			    << "Error: The module contains a moduleinfo.json file and the module factory exports a IPluginCompatibility class. Only one is allowed, while the moduleinfo.json one is prefered.\n";
		}
	}
	StringStream strStream;
	if (compat->getCompatibilityJSON (&strStream) != kResultTrue)
	{
		if (errorStream)
		{
			*errorStream
			    << "Error: Call to IPluginCompatiblity::getCompatibilityJSON (IBStream*) failed\n";
		}
		failure = true;
	}
	else if (auto result = ModuleInfoLib::parseCompatibilityJson (strStream.str, errorStream))
	{
		// TODO: Check that the "New" classes are part of the Module;
	}
	else
	{
		failure = true;
	}
	return !failure;
}

//------------------------------------------------------------------------
} // Steinberg
