//-----------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : VST SDK
//
// Category    : VST3Inspector
// Filename    : public.sdk/samples/vst-hosting/inspectorapp/window.cpp
// Created by  : Steinberg, 01/2021
// Description : VST3 Inspector Application
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
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#include "window.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/standalone/include/helpers/menubuilder.h"
#include "vstgui/standalone/include/helpers/uidesc/customization.h"
#include "vstgui/standalone/include/helpers/value.h"
#include "vstgui/standalone/include/helpers/valuelistener.h"
#include "vstgui/standalone/include/helpers/windowcontroller.h"
#include "vstgui/standalone/include/ialertbox.h"
#include "vstgui/standalone/include/iapplication.h"
#include "vstgui/standalone/include/iasync.h"
#include "vstgui/standalone/include/iuidescwindow.h"
#include "vstgui/uidescription/cstream.h"
#include "vstgui/uidescription/delegationcontroller.h"
#include "vstgui/uidescription/iuidescription.h"
#include "vstgui/uidescription/uiattributes.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/moduleinfo/moduleinfo.h"
#include "public.sdk/source/vst/moduleinfo/moduleinfocreator.h"
#include "public.sdk/source/vst/moduleinfo/moduleinfoparser.h"
#include <cassert>
#include <fstream>

//------------------------------------------------------------------------
namespace VST3Inspector {
namespace {

//------------------------------------------------------------------------
std::string loadFile (const std::string& path)
{
	std::ifstream file (path, std::ios_base::in | std::ios_base::binary);
	if (!file.is_open ())
		return {};
	auto size = file.seekg (0, std::ios_base::end).tellg ();
	file.seekg (0, std::ios_base::beg);
	std::string data;
	data.resize (size);
	file.read (data.data (), data.size ());
	if (file.bad ())
		return {};
	return data;
}

//------------------------------------------------------------------------
} // anonymous

using namespace VSTGUI;
using namespace VSTGUI::Standalone;
using namespace Steinberg;

//------------------------------------------------------------------------
class DummyFactory : public IPluginFactory
{
public:
	static DummyFactory* instance ()
	{
		static DummyFactory gInstance;
		return &gInstance;
	}

private:
	tresult PLUGIN_API getFactoryInfo (PFactoryInfo* info) override
	{
		*info = {};
		return kResultTrue;
	}
	int32 PLUGIN_API countClasses () override { return 0; }
	tresult PLUGIN_API getClassInfo (int32 index, PClassInfo* info) override
	{
		return kResultFalse;
	}
	tresult PLUGIN_API createInstance (FIDString cid, FIDString _iid, void** obj) override
	{
		return kNoInterface;
	}
	tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) override
	{
		QUERY_INTERFACE (_iid, obj, IPluginFactory::iid, IPluginFactory)
		QUERY_INTERFACE (_iid, obj, FUnknown::iid, FUnknown)
		return kNoInterface;
	}
	uint32 PLUGIN_API addRef () override { return 1000; }
	uint32 PLUGIN_API release () override { return 1000; }
};

//------------------------------------------------------------------------
class InvalidModule : public VST3::Hosting::Module
{
public:
	InvalidModule () { factory = VST3::Hosting::PluginFactory (DummyFactory::instance ()); }

private:
	bool load (const std::string& path, std::string& errorDescription) override { return true; };
};

//------------------------------------------------------------------------
struct ValueMap
{
	ValuePtr addValue (const ValuePtr& value)
	{
		auto index = valueList.size ();
		valueList.push_back (value);
		valueMap.insert ({value->getID ().getString (), index});
		return value;
	}

	ValuePtr get (const std::string& ID)
	{
		auto it = valueMap.find (ID);
		if (it != valueMap.end ())
			return valueList.at (it->second);
		return nullptr;
	}

	template <typename T>
	auto get (const std::string& ID)
	{
		if (auto value = get (ID))
		{
			return std::dynamic_pointer_cast<T> (value);
		}
		return std::shared_ptr<T> (nullptr);
	}

	const UIDesc::IModelBinding::ValueList& getValues () const { return valueList; }

private:
	UIDesc::IModelBinding::ValueList valueList;
	std::unordered_map<std::string, size_t> valueMap;
};

//------------------------------------------------------------------------
static constexpr auto ModulePathListID = "ModulePathList";
static constexpr auto ModulePathID = "Module::Path";
static constexpr auto FactoryVendorID = "Factory::Vendor";
static constexpr auto FactoryUrlID = "Factory::URL";
static constexpr auto FactoryEMailID = "Factory::EMail";
static constexpr auto FactoryFlagsID = "Factory::Flags";
static constexpr auto ClassInfoListID = "ClassInfoList";
static constexpr auto ClassInfoClassID = "ClassInfo::ClassID";
static constexpr auto ClassInfoCategoryID = "ClassInfo::Category";
static constexpr auto ClassInfoNameID = "ClassInfo::Name";
static constexpr auto ClassInfoVendorID = "ClassInfo::Vendor";
static constexpr auto ClassInfoVersionID = "ClassInfo::Version";
static constexpr auto ClassInfoSDKVersionID = "ClassInfo::SDKVersion";
static constexpr auto ClassInfoSubCategoriesID = "ClassInfo::SubCategories";
static constexpr auto ClassInfoCardinalityID = "ClassInfo::Cardinality";
static constexpr auto ClassInfoClassFlagsID = "ClassInfo::ClassFlags";

using namespace VST3::Hosting;

//------------------------------------------------------------------------
struct SnapshotController : DelegationController, NonAtomicReferenceCounted
{
	SnapshotController (IController* parent) : DelegationController (parent) {}
	~SnapshotController () noexcept {}

	CView* createView (const UIAttributes& attributes, const IUIDescription* description) override
	{
		if (auto viewName = attributes.getAttributeValue (IUIDescription::kCustomViewName))
		{
			if (*viewName == "ImageView")
			{
				imageView = VSTGUI::shared (new CView (CRect ()));
				return imageView;
			}
		}
		return controller->createView (attributes, description);
	}

	void setSnapshot (const std::string& moduleBasePath,
	                  const ModuleInfo::SnapshotList* newSnapshot)
	{
		snapshot = newSnapshot ? *newSnapshot : ModuleInfo::SnapshotList ();
		if (snapshot.empty ())
		{
			imageView->setBackground (nullptr);
			imageView->setViewSize ({});
		}
		else
		{
			SharedPointer<CBitmap> bitmap;
			const auto& factory = getPlatformFactory ();
			for (const auto& imageDesc : snapshot)
			{
				if (auto pb = factory.createBitmapFromPath (
				        (moduleBasePath + "/" + imageDesc.path).data ()))
				{
					pb->setScaleFactor (imageDesc.scaleFactor);
					if (bitmap)
						bitmap->addBitmap (pb);
					else
						bitmap = makeOwned<CBitmap> (pb);
				}
			}
			imageView->setBackground (bitmap);
			if (bitmap)
			{
				auto viewSize = imageView->getViewSize ();
				viewSize.setSize (bitmap->getSize ());
				imageView->setViewSize (viewSize);
			}
		}
	}

	SharedPointer<CView> imageView;
	ModuleInfo::SnapshotList snapshot;
};

//------------------------------------------------------------------------
struct WindowController : WindowControllerAdapter,
                          UIDesc::CustomizationAdapter,
                          UIDesc::IModelBinding,
                          MenuBuilderAdapter,
                          ValueListenerAdapter
{
	WindowController ()
	{
		values.addValue (Value::makeStringListValue (ModulePathListID, {"", ""}))
		    ->registerListener (this);

		// Module Values
		values.addValue (Value::makeStringValue (ModulePathID, ""));
		// Factory Values
		values.addValue (Value::makeStringValue (FactoryVendorID, ""));
		values.addValue (Value::makeStringValue (FactoryUrlID, ""));
		values.addValue (Value::makeStringValue (FactoryEMailID, ""));
		values.addValue (Value::makeStringValue (FactoryFlagsID, ""));

		values.addValue (Value::makeStringListValue (ClassInfoListID, {"", ""}))
		    ->registerListener (this);
		// Class Info Values
		values.addValue (Value::makeStringValue (ClassInfoClassID, ""));
		values.addValue (Value::makeStringValue (ClassInfoCategoryID, ""));
		values.addValue (Value::makeStringValue (ClassInfoNameID, ""));
		values.addValue (Value::makeStringValue (ClassInfoVendorID, ""));
		values.addValue (Value::makeStringValue (ClassInfoVersionID, ""));
		values.addValue (Value::makeStringValue (ClassInfoSDKVersionID, ""));
		values.addValue (Value::makeStringValue (ClassInfoSubCategoriesID, ""));
		values.addValue (Value::makeStringValue (ClassInfoCardinalityID, ""));
		values.addValue (Value::makeStringValue (ClassInfoClassFlagsID, ""));

		Async::schedule (Async::backgroundQueue (), [this] () {
			auto modulePaths = Module::getModulePaths ();
			Async::schedule (Async::mainQueue (),
			                 [this, paths = std::move (modulePaths)] () mutable {
				                 setModulePaths (std::move (paths));
			                 });
		});
	}

	static Optional<std::string> lastPathComponent (const std::string& path)
	{
#if SMTG_WINDOWS
		static constexpr auto pathSeparator = '\\';
#else
		static constexpr auto pathSeparator = '/';
#endif
		size_t sepPos = path.find_last_of (pathSeparator);
		if (sepPos == std::string::npos)
			return {};
		return {path.substr (sepPos + 1)};
	}

	void setModulePaths (Module::PathList&& pathList)
	{
		modulePathList = std::move (pathList);
		std::sort (modulePathList.begin (), modulePathList.end (),
		           [] (const auto& lhs, const auto& rhs) {
			           auto lhsName = lastPathComponent (lhs);
			           auto rhsName = lastPathComponent (rhs);
			           if (lhsName && rhsName)
			           {
				           std::for_each (lhsName->begin (), lhsName->end (),
				                          [] (auto& c) { c = std::tolower (c); });
				           std::for_each (rhsName->begin (), rhsName->end (),
				                          [] (auto& c) { c = std::tolower (c); });
				           return *lhsName < *rhsName;
			           }
			           return lhs < rhs;
		           });
		moduleInfos.clear ();
		moduleInfos.resize (modulePathList.size ());

		IStringListValue::StringList nameList;
		for (const auto& path : modulePathList)
		{
			if (auto name = lastPathComponent (path))
				nameList.emplace_back (UTF8String (*name));
			else
				nameList.emplace_back (UTF8String (path));
		}
		if (auto value = values.get<IStringListValue> (ModulePathListID))
			value->updateStringList (nameList);
		if (auto value = values.get (ModulePathListID))
			Value::performSingleEdit (*value, 0.);
	}

	void onEndEdit (IValue& value) override
	{
		if (value.getID () == ModulePathListID)
		{
			onModuleSelection (value.getConverter ().normalizedToPlain (value.getValue ()));
		}
		if (value.getID () == ClassInfoListID)
		{
			onClassInfoSelection (value.getConverter ().normalizedToPlain (value.getValue ()));
		}
	}

	void setStringValue (const std::string& valueID, const std::string& string)
	{
		auto value = values.get (valueID);
		if (value)
			Value::performStringValueEdit (*value, UTF8String (string));
	}

	std::string createFlagsString (int32_t flags) const
	{
		std::string flagsString = "0b";
		for (int32_t i = 31; i >= 0; --i)
		{
			if (flags & (1 << i))
				flagsString += "1";
			else
				flagsString += "0";
		}
		return flagsString;
	}

	std::string createSubCategoryString (const ModuleInfo::ClassInfo& info) const
	{
		std::string result;
		for (const auto& cat : info.subCategories)
		{
			if (result.empty () == false)
				result += "|";
			result += cat;
		}
		return result;
	}

	void onClassInfoSelection (uint32_t index)
	{
		if (index >= moduleInfos[currentSelectedModule].classes.size ())
		{
			setStringValue (ClassInfoClassID, "");
			setStringValue (ClassInfoCategoryID, "");
			setStringValue (ClassInfoNameID, "");
			setStringValue (ClassInfoVendorID, "");
			setStringValue (ClassInfoVersionID, "");
			setStringValue (ClassInfoSDKVersionID, "");
			setStringValue (ClassInfoSubCategoriesID, "");
			setStringValue (ClassInfoCardinalityID, "");
			setStringValue (ClassInfoClassFlagsID, "");
			if (snapShotController)
				snapShotController->setSnapshot ("", nullptr);
			return;
		}
		const auto& cl = moduleInfos[currentSelectedModule].classes[index];
		setStringValue (ClassInfoClassID, cl.cid);
		setStringValue (ClassInfoCategoryID, cl.category);
		setStringValue (ClassInfoNameID, cl.name);
		setStringValue (ClassInfoVendorID, cl.vendor);
		setStringValue (ClassInfoVersionID, cl.version);
		setStringValue (ClassInfoSDKVersionID, cl.sdkVersion);
		setStringValue (ClassInfoSubCategoriesID, createSubCategoryString (cl));
		if (cl.cardinality == Steinberg::PClassInfo::ClassCardinality::kManyInstances)
			setStringValue (ClassInfoCardinalityID, "");
		else
			setStringValue (ClassInfoCardinalityID, std::to_string (cl.cardinality));
		setStringValue (ClassInfoClassFlagsID, createFlagsString (cl.flags));
		if (snapShotController)
			snapShotController->setSnapshot (modulePathList[currentSelectedModule], &cl.snapshots);
	}

	void updateModuleInfo (uint32_t index)
	{
		const auto& modulePath = modulePathList[index];
		if (auto moduleInfoPath = Module::getModuleInfoPath (modulePath))
		{
			auto moduleJson = loadFile (*moduleInfoPath);
			if (auto moduleInfo = ModuleInfoLib::parseJson (moduleJson, nullptr))
			{
				moduleInfos[index] = *moduleInfo;
			}
		}
		else
		{
			std::string errorDesc;
			if (auto module = Module::create (modulePath, errorDesc))
			{
				moduleInfos[index] = ModuleInfoLib::createModuleInfo (*module.get (), true);
			}
		}
	}

	void onModuleSelection (uint32_t index)
	{
		if (index >= modulePathList.size ())
			return;
		currentSelectedModule = index;
		if (moduleInfos[index].name.empty ())
		{
			updateModuleInfo (index);
		}
		const auto& moduleInfo = moduleInfos[index];
		const auto& modulePath = modulePathList[index];
		setStringValue (ModulePathID, modulePath);
		setStringValue (FactoryVendorID, moduleInfo.factoryInfo.vendor);
		setStringValue (FactoryUrlID, moduleInfo.factoryInfo.url);
		setStringValue (FactoryEMailID, moduleInfo.factoryInfo.email);
		setStringValue (FactoryFlagsID, createFlagsString (moduleInfo.factoryInfo.flags));
		IStringListValue::StringList classInfoList;
		for (const auto& classInfo : moduleInfo.classes)
		{
			classInfoList.emplace_back (classInfo.name);
		}
		if (auto value = values.get<IStringListValue> (ClassInfoListID))
		{
			value->updateStringList (classInfoList);
		}
		// select first class info
		if (auto value = values.get (ClassInfoListID))
			Value::performSingleEdit (*value, 0.);
	}

	const ValueList& getValues () const override { return values.getValues (); }

	IController* createController (const UTF8StringView& name, IController* parent,
	                               const IUIDescription* uiDesc) override
	{
		if (name == "SnapshotViewController")
		{
			snapShotController = VSTGUI::shared (new SnapshotController (parent));
			return snapShotController;
		}
		return nullptr;
	}

	bool showCommandInMenu (const Interface& context, const Command& cmd) const override
	{
		return false;
	}

private:
	ValueMap values;
	ValuePtr modulePathListValue;
	SharedPointer<SnapshotController> snapShotController;

	using ModuleInfoList = std::vector<ModuleInfo>;

	Module::PathList modulePathList;
	ModuleInfoList moduleInfos;
	uint32_t currentSelectedModule {0};
};

//------------------------------------------------------------------------
WindowPtr makeWindow ()
{
	auto controller = std::make_shared<WindowController> ();

	UIDesc::Config config;
	config.uiDescFileName = "window.uidesc";
	config.viewName = "MainWindow";
	config.windowConfig.autoSaveFrameName = "MainWindow";
	config.windowConfig.type = WindowType::Document;
	config.windowConfig.style = WindowStyle ().border ().close ().centered ().size ();
	config.windowConfig.title = "VST3 Inspector";
	config.customization = controller;
	config.modelBinding = controller;
	return UIDesc::makeWindow (config);
}

//------------------------------------------------------------------------
} // VST3Inspector
