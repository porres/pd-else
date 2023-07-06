//------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/auwrapper/aucocoaview.mm
// Created by  : Steinberg, 12/2007
// Description : VST 3 -> AU Wrapper
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

/// \cond ignore

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#import "aucocoaview.h"
#import "auwrapper.h"
#import "public.sdk/source/vst/utility/objcclassbuilder.h"
#import "pluginterfaces/base/funknownimpl.h"
#import "pluginterfaces/gui/iplugview.h"
#import "pluginterfaces/vst/ivsteditcontroller.h"

//------------------------------------------------------------------------
@interface NSObject (SMTG_AUView)
- (id)initWithEditController:(Steinberg::Vst::IEditController*)editController
                   audioUnit:(AudioUnit)au
               preferredSize:(NSSize)size;
@end

//------------------------------------------------------------------------
namespace Steinberg {
namespace {

//------------------------------------------------------------------------
struct AUPlugFrame : U::Implements<U::Directly<IPlugFrame>>
{
	AUPlugFrame (NSView* parent) : parent (parent) {}
	tresult PLUGIN_API resizeView (IPlugView* view, ViewRect* vr) override
	{
		NSRect newSize = NSMakeRect ([parent frame].origin.x, [parent frame].origin.y,
		                             vr->right - vr->left, vr->bottom - vr->top);
		[parent setFrame:newSize];
		return kResultTrue;
	}

	NSView* parent;
};

//------------------------------------------------------------------------
struct AUView
{
	static constexpr auto VarNamePlugView = "plugView";
	static constexpr auto VarNameEditController = "editController";
	static constexpr auto VarNameAudioUnit = "audioUnit";
	static constexpr auto VarNameDynlib = "dynlib";
	static constexpr auto VarNamePlugFrame = "plugFrame";
	static constexpr auto VarNameIsAttached = "isAttached";

	struct Instance : ObjCInstance
	{
		using PlugViewVar = std::optional<ObjCVariable<IPlugView*>>;
		using EditControllerVar = std::optional<ObjCVariable<Vst::IEditController*>>;
		using AudioUnitVar = std::optional<ObjCVariable<AudioUnit>>;
		using DynlibVar = std::optional<ObjCVariable<FObject*>>;
		using PlugFrameVar = std::optional<ObjCVariable<AUPlugFrame*>>;
		using IsAttachedVar = std::optional<ObjCVariable<BOOL>>;

		Instance (__unsafe_unretained id obj) : ObjCInstance (obj, [NSView class])
		{
			plugView = getVariable<IPlugView*> (VarNamePlugView);
			editController = getVariable<Vst::IEditController*> (VarNameEditController);
			audioUnit = getVariable<AudioUnit> (VarNamePlugView);
			dynlib = getVariable<FObject*> (VarNameDynlib);
			plugFrame = getVariable<AUPlugFrame*> (VarNamePlugFrame);
			isAttached = getVariable<BOOL> (VarNameIsAttached);
		}

		PlugViewVar plugView;
		EditControllerVar editController;
		AudioUnitVar audioUnit;
		DynlibVar dynlib;
		PlugFrameVar plugFrame;
		IsAttachedVar isAttached;
	};

	static id alloc ()
	{
		static ObjCClass gInstance;
		return [gInstance.cl alloc];
	}

private:
	struct ObjCClass
	{
		Class cl;

		ObjCClass ()
		{
			cl = ObjCClassBuilder ()
			         .init ("SMTG_AUView", [NSView class])
			         .addIvar<IPlugView*> (VarNamePlugView)
			         .addIvar<Vst::IEditController*> (VarNameEditController)
			         .addIvar<AudioUnit> (VarNameAudioUnit)
			         .addIvar<FObject*> (VarNameDynlib)
			         .addIvar<AUPlugFrame*> (VarNamePlugFrame)
			         .addIvar<BOOL> (VarNameIsAttached)
			         .addMethod (@selector (initWithEditController:audioUnit:preferredSize:),
			                     initWithEditController)
			         .addMethod (@selector (setFrame:), setFrame)
			         .addMethod (@selector (isFlipped), isFlipped)
			         .addMethod (@selector (viewDidMoveToSuperview), viewDidMoveToSuperview)
			         .addMethod (@selector (dealloc), dealloc)
			         .finalize ();
		}

		static id initWithEditController (id self, SEL cmd, Vst::IEditController* editController,
		                                  AudioUnit au, NSSize size)
		{
			ObjCInstance obj (self);
			self = obj.callSuper<id (NSRect), id> (@selector (initWithFrame:),
			                                       NSMakeRect (0, 0, size.width, size.height));
			if (self)
			{
				Instance inst (self);
				inst.editController->set (editController);
				editController->addRef ();
				inst.audioUnit->set (au);
				auto plugView = editController->createView (Vst::ViewType::kEditor);
				if (!plugView ||
				    plugView->isPlatformTypeSupported (kPlatformTypeNSView) != kResultTrue)
				{
					[self dealloc];
					return nil;
				}
				inst.plugView->set (plugView);
				auto plugFrame = NEW AUPlugFrame (self);
				inst.plugFrame->set (plugFrame);
				plugView->setFrame (plugFrame);

				if (plugView->attached (self, kPlatformTypeNSView) != kResultTrue)
				{
					[self dealloc];
					return nil;
				}
				ViewRect vr;
				if (plugView->getSize (&vr) == kResultTrue)
				{
					NSRect newSize = NSMakeRect (0, 0, vr.right - vr.left, vr.bottom - vr.top);
					[self setFrame:newSize];
				}
				inst.isAttached->set (YES);
				FObject* fObject = nullptr;
				UInt32 size = sizeof (FObject*);
				if (AudioUnitGetProperty (au, 64001, kAudioUnitScope_Global, 0, &fObject, &size) ==
				    noErr)
				{
					fObject->addRef ();
					inst.dynlib->set (fObject);
				}
			}
			return self;
		}

		static void setFrame (id self, SEL cmd, NSRect newSize)
		{
			Instance inst (self);
			inst.callSuper<void (NSRect)> (@selector (setFrame:), newSize);
			ViewRect viewRect (0, 0, newSize.size.width, newSize.size.height);

			if (inst.plugView->get ())
				inst.plugView->get ()->onSize (&viewRect);
		}

		static BOOL isFlipped (id self, SEL cmd) { return YES; }

		static void viewDidMoveToSuperview (id self, SEL cmd)
		{
			Instance inst (self);
			if (inst.plugView->get ())
			{
				if ([self superview])
				{
					if (!inst.isAttached->get ())
					{
						if (inst.plugView->get ()->attached (self, kPlatformTypeNSView) ==
						    kResultTrue)
						{
							inst.isAttached->set (YES);
						}
					}
				}
				else
				{
					if (inst.isAttached->get ())
					{
						inst.plugView->get ()->removed ();
						inst.isAttached->set (NO);
					}
				}
			}
		}

		static void dealloc (id self, SEL cmd)
		{
			Instance inst (self);
			if (auto plugView = inst.plugView->get ())
			{
				if (inst.isAttached->get ())
				{
					plugView->setFrame (0);
					plugView->removed ();
				}
				plugView->release ();
				if (auto plugFrame = inst.plugFrame->get ())
					plugFrame->release ();

				if (auto editController = inst.editController->get ())
				{
					auto refCount = editController->addRef ();
					if (refCount == 2)
						editController->terminate ();

					editController->release ();
					editController->release ();
					inst.editController->set (nullptr);
				}
			}
			if (auto dynlib = inst.dynlib->get ())
				dynlib->release ();
			inst.callSuper<void ()> (@selector (dealloc));
		}
	};
};

//------------------------------------------------------------------------
} // anonymous
} // Steinberg

//------------------------------------------------------------------------
@implementation SMTG_AUCocoaUIBase_CLASS_NAME

//------------------------------------------------------------------------
- (unsigned)interfaceVersion
{
	return 0;
}

//------------------------------------------------------------------------
- (NSString*)description
{
	return @"Cocoa View";
}

//------------------------------------------------------------------------
- (NSView*)uiViewForAudioUnit:(AudioUnit)inAU withSize:(NSSize)inPreferredSize
{
	using namespace Steinberg;

	Vst::IEditController* editController = 0;
	UInt32 size = sizeof (Vst::IEditController*);
	if (AudioUnitGetProperty (inAU, 64000, kAudioUnitScope_Global, 0, &editController, &size) !=
	    noErr)
		return nil;
	return [[AUView::alloc () initWithEditController:editController
	                                       audioUnit:inAU
	                                   preferredSize:inPreferredSize] autorelease];
}

@end

/// \endcond
