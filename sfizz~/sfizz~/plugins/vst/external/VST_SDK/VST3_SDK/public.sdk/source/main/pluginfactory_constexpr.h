//------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : SDK Core
//
// Category    : Common Base Classes
// Filename    : public.sdk/source/main/pluginfactory_constexpr.h
// Created by  : Steinberg, 10/2021
// Description : Standard Plug-In Factory (constexpr variant)
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

#pragma once

#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/base/ipluginbase.h"

#if !SMTG_CPP17
#error "C++17 is required for this header"
#endif

#include <array>

namespace Steinberg {

//------------------------------------------------------------------------
/** IPluginFactory implementation with compile time provided factory and class infos.
\ingroup sdkBase
\see \ref constexprClassFactoryMacros

You can use this factory when your number of classes are known during compile time. The
advantage here is that during runtime no unnecessary setup is needed.

Please note that this only supports ASCII names and thus if you need to support Unicode names
you must use another implementation.

This only works when compiling with c++17 or newer.
*/
template <typename T>
class PluginFactory
: public U::ImplementsNonDestroyable<U::Directly<IPluginFactory2>, U::Indirectly<IPluginFactory>>
{
public:
//------------------------------------------------------------------------
	tresult PLUGIN_API getFactoryInfo (PFactoryInfo* info) override
	{
		if (info == nullptr)
			return kInvalidArgument;
		*info = T::factoryInfo;
		return kResultTrue;
	}
	int32 PLUGIN_API countClasses () override { return static_cast<int32> (T::classInfos.size ()); }
	tresult PLUGIN_API getClassInfo (int32 index, PClassInfo* info) override
	{
		if (index < 0 || index >= static_cast<int32> (T::classInfos.size ()) || info == nullptr)
			return kInvalidArgument;
		*info = {};
		const auto& ci = T::classInfos[index];
		copyTUID (info->cid, ci.cid);
		info->cardinality = ci.cardinality;
		if (ci.category)
			strncpy (info->category, ci.category, PClassInfo::kCategorySize);
		if (ci.name)
			strncpy (info->name, ci.name, PClassInfo::kNameSize);
		return kResultTrue;
	}
	tresult PLUGIN_API createInstance (FIDString cid, FIDString iid, void** obj) override
	{
		for (const auto& e : T::classInfos)
		{
			if (FUnknownPrivate::iidEqual (e.cid, cid))
			{
				auto instance = e.create (e.context);
				if (instance)
				{
					if (instance->queryInterface (iid, obj) == kResultOk)
					{
						instance->release ();
						return kResultOk;
					}
					else
						instance->release ();
				}
			}
		}
		*obj = nullptr;
		return kNoInterface;
	}
	tresult PLUGIN_API getClassInfo2 (int32 index, PClassInfo2* info) override
	{
		if (index < 0 || index >= static_cast<int32> (T::classInfos.size ()) || info == nullptr)
			return kInvalidArgument;
		*info = T::classInfos[index];
		return kResultTrue;
	}
};

//------------------------------------------------------------------------
namespace PluginFactoryDetail {

//------------------------------------------------------------------------
struct ClassInfo2WithCreateFunc : PClassInfo2
{
	using CreateInstanceFunc = FUnknown* (*)(void*);
	CreateInstanceFunc create {nullptr};
	void* context {nullptr};
};

//------------------------------------------------------------------------
inline constexpr ClassInfo2WithCreateFunc makeClassInfo2 (
    const TUID cid, int32 cardinality, const char8* category, const char8* name, int32 classFlags,
    const char8* subCategories, const char8* vendor, const char8* version, const char8* sdkVersion,
    ClassInfo2WithCreateFunc::CreateInstanceFunc func, void* context = nullptr)
{
	ClassInfo2WithCreateFunc classInfo {};
	copyTUID (classInfo.cid, cid);
	classInfo.cardinality = cardinality;
	strncpy8 (classInfo.category, category, PClassInfo::kCategorySize);
	strncpy8 (classInfo.name, name, PClassInfo::kNameSize);
	classInfo.classFlags = classFlags;
	if (subCategories)
		strncpy8 (classInfo.subCategories, subCategories, PClassInfo2::kSubCategoriesSize);
	if (vendor)
		strncpy8 (classInfo.vendor, vendor, PClassInfo2::kVendorSize);
	strncpy8 (classInfo.version, version, PClassInfo2::kVersionSize);
	strncpy8 (classInfo.sdkVersion, sdkVersion, PClassInfo2::kVersionSize);
	classInfo.create = func;
	classInfo.context = context;
	return classInfo;
}

//------------------------------------------------------------------------
} // namespace PluginFactoryDetail
} // namespace Steinberg

#ifdef BEGIN_FACTORY_DEF
#undef BEGIN_FACTORY_DEF
#endif

/** \defgroup constexprClassFactoryMacros Macros for defining the compile time class factory
\ingroup sdkBase

\b Example:

\code

static constexpr size_t numberOfClasses = 1;
static DECLARE_UID (TestPluginUID, 0x00000001, 0x00000002, 0x00000003, 0x00000004);

BEGIN_FACTORY_DEF ("MyCompany", "mycompany.com", "info@mycompany.com", numberOfClasses)

    DEF_CLASS (TestPluginUID,
               PClassInfo::kManyInstances,
               "PlugIn",
               "Test PlugIn",
               0,
               "SubCategory",
               "1.0.0",
               stringSDKVersion,
               TestPlugin::newInstance,
               nullptr)

END_FACTORY

\endcode

@{*/

// clang-format off
#define BEGIN_FACTORY_DEF(company, url, email, noClasses)                                     \
	struct SMTG_HIDDEN_SYMBOL FactoryData                                                     \
	{                                                                                         \
		static constexpr Steinberg::PFactoryInfo factoryInfo = {                              \
		    company, url, email, Steinberg::PFactoryInfo::kUnicode};                          \
                                                                                              \
		static constexpr std::array<Steinberg::PluginFactoryDetail::ClassInfo2WithCreateFunc, \
		                            noClasses>                                                \
		    classInfos = {

#define DEF_CLASS(cid, cardinality, category, name, classFlags, subCategories, version,           \
                  sdkVersion, createMethod, createContext)                                        \
	Steinberg::PluginFactoryDetail::makeClassInfo2 (cid, cardinality, category, name, classFlags, \
	                                                subCategories, nullptr, version, sdkVersion,  \
	                                                createMethod, createContext),

#define END_FACTORY                                                              \
	};};                                                                         \
	SMTG_EXPORT_SYMBOL Steinberg::IPluginFactory* PLUGIN_API GetPluginFactory () \
	{                                                                            \
		static Steinberg::PluginFactory<FactoryData> factory;                    \
		return &factory;                                                         \
	}
// clang-format on

/** @} */
