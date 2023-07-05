//------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/vstguieditor.cpp
// Created by  : Steinberg, 04/2005
// Description : VSTGUI Editor
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

#include "vstguieditor.h"
#include "public.sdk/source/main/moduleinit.h"
#include "pluginterfaces/base/keycodes.h"

#if SMTG_OS_WINDOWS && SMTG_MODULE_IS_BUNDLE
#include "vstgui_win32_bundle_support.h"
#endif

#define VSTGUI_NEEDS_INIT_LIB ((VSTGUI_VERSION_MAJOR == 4 && VSTGUI_VERSION_MINOR > 9) || (VSTGUI_VERSION_MAJOR > 4))

#if VSTGUI_VERSION_MAJOR < 4
#include "vstgui/vstkeycode.h"
#endif

#if VSTGUI_NEWER_THAN_4_10
#include "vstgui/lib/events.h"
#include "vstgui/lib/platform/iplatformframe.h"
#endif

#include "base/source/fstring.h"

#if VSTGUI_NEEDS_INIT_LIB
#include "vstgui/lib/vstguiinit.h"
static Steinberg::ModuleInitializer InitVSTGUI ([] () {
	using namespace Steinberg;
	VSTGUI::init (getPlatformModuleHandle ());
#if SMTG_MODULE_IS_BUNDLE
	Vst::setupVSTGUIBundleSupport (getPlatformModuleHandle ());
#endif // SMTG_MODULE_IS_BUNDLE
});
static Steinberg::ModuleTerminator TermVSTGUI ([] () { VSTGUI::exit (); });
#else

#if SMTG_OS_MACOS
namespace VSTGUI { void* gBundleRef = nullptr; }
#elif SMTG_OS_WINDOWS
void* hInstance = nullptr; // VSTGUI hInstance
extern void* moduleHandle;
#elif SMTG_OS_LINUX
extern void* moduleHandle;
namespace VSTGUI {
void* soHandle = nullptr;
} // VSTGUI
#endif // SMTG_OS_MACOS

//------------------------------------------------------------------------
static Steinberg::ModuleInitializer InitVSTGUIEditor ([] () {
	using namespace Steinberg;
#if SMTG_OS_MACOS || SMTG_OS_IOS
	VSTGUI::gBundleRef = getPlatformModuleHandle ();
#elif SMTG_OS_WINDOWS
	hInstance = getPlatformModuleHandle ();
#if SMTG_MODULE_IS_BUNDLE
	Vst::setupVSTGUIBundleSupport (hInstance);
#endif // SMTG_MODULE_IS_BUNDLE
#elif SMTG_OS_LINUX
	VSTGUI::soHandle = getPlatformModuleHandle ();
#endif
});

#endif // VSTGUI_NEEDS_INIT_LIB

using namespace VSTGUI;

namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
VSTGUIEditor::VSTGUIEditor (void* controller, ViewRect* size)
: EditorView (static_cast<EditController*> (controller), size)
{
	// create a timer used for idle update: will call notify method
	timer = new CVSTGUITimer (dynamic_cast<CBaseObject*> (this));
}

//------------------------------------------------------------------------
VSTGUIEditor::~VSTGUIEditor ()
{
	if (timer)
		timer->forget ();
}

//------------------------------------------------------------------------
void VSTGUIEditor::setIdleRate (int32 millisec)
{
	if (timer)
		timer->setFireTime (millisec);
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::isPlatformTypeSupported (FIDString type)
{
#if SMTG_OS_WINDOWS
	if (strcmp (type, kPlatformTypeHWND) == 0)
		return kResultTrue;

#elif SMTG_OS_MACOS
#if TARGET_OS_IPHONE
	if (strcmp (type, kPlatformTypeUIView) == 0)
		return kResultTrue;
#else
#if MAC_CARBON
	if (strcmp (type, kPlatformTypeHIView) == 0)
		return kResultTrue;
#endif

#if MAC_COCOA
	if (strcmp (type, kPlatformTypeNSView) == 0)
		return kResultTrue;
#endif
#endif
#elif SMTG_OS_LINUX
	if (strcmp (type, kPlatformTypeX11EmbedWindowID) == 0)
		return kResultTrue;
#endif

	return kInvalidArgument;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::attached (void* parent, FIDString type)
{
#if SMTG_OS_MACOS
	if (isPlatformTypeSupported (type) != kResultTrue)
		return kResultFalse;

#if MAC_COCOA && MAC_CARBON && !(VSTGUI_VERSION_MAJOR >= 4 && VSTGUI_VERSION_MINOR >= 1)
	CFrame::setCocoaMode (strcmp (type, kPlatformTypeNSView) == 0);
#endif
#endif

#if VSTGUI_VERSION_MAJOR >= 4 && VSTGUI_VERSION_MINOR >= 1
	PlatformType platformType = PlatformType::kDefaultNative;
#if SMTG_OS_MACOS
#if TARGET_OS_IPHONE
	if (strcmp (type, kPlatformTypeUIView) == 0)
		platformType = PlatformType::kUIView;
#else
#if MAC_CARBON
	if (strcmp (type, kPlatformTypeHIView) == 0)
		platformType = PlatformType::kWindowRef;
#endif

#if MAC_COCOA
	if (strcmp (type, kPlatformTypeNSView) == 0)
		platformType = PlatformType::kNSView;
#endif
#endif
#endif // SMTG_OS_MACOS
	if (open (parent, platformType) == true)
#else
	if (open (parent) == true)
#endif
	{
		ViewRect vr (0, 0, (int32)frame->getWidth (), (int32)frame->getHeight ());
		setRect (vr);
		if (plugFrame)
			plugFrame->resizeView (this, &vr);

		if (timer)
			timer->start ();
	}
	return EditorView::attached (parent, type);
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::removed ()
{
	if (timer)
		timer->stop ();

	close ();
	return EditorView::removed ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::onSize (ViewRect* newSize)
{
	if (frame)
		frame->setSize (newSize->right - newSize->left, newSize->bottom - newSize->top);

	return EditorView::onSize (newSize);
}

//------------------------------------------------------------------------
void VSTGUIEditor::beginEdit (VSTGUI_INT32 index)
{
	if (controller)
		controller->beginEdit (index);
}

//------------------------------------------------------------------------
void VSTGUIEditor::endEdit (VSTGUI_INT32 index)
{
	if (controller)
		controller->endEdit (index);
}

//------------------------------------------------------------------------
VSTGUI_INT32 VSTGUIEditor::getKnobMode () const
{
	switch (EditController::getHostKnobMode ())
	{
		case kRelativCircularMode: return VSTGUI::kRelativCircularMode;
		case kLinearMode: return VSTGUI::kLinearMode;
	}
	return VSTGUI::kCircularMode;
}

//------------------------------------------------------------------------
CMessageResult VSTGUIEditor::notify (CBaseObject* /*sender*/, const char* message)
{
	if (message == CVSTGUITimer::kMsgTimer)
	{
		if (frame)
			frame->idle ();

		return kMessageNotified;
	}

	return kMessageUnknown;
}

#if VSTGUI_NEWER_THAN_4_10
//------------------------------------------------------------------------
static KeyboardEvent translateKeyMessage (char16 key, int16 keyMsg, int16 modifiers)
{
	KeyboardEvent event;
	event.virt = static_cast<VirtualKey> (keyMsg);
	if (key == 0)
		key = VirtualKeyCodeToChar ((uint8)keyMsg);
	if (key)
		event.character = key;
	if (modifiers)
	{
		if (modifiers & kShiftKey)
			event.modifiers.add (ModifierKey::Shift);
		if (modifiers & kAlternateKey)
			event.modifiers.add (ModifierKey::Alt);
		if (modifiers & kCommandKey)
			event.modifiers.add (ModifierKey::Control);
		if (modifiers & kControlKey)
			event.modifiers.add (ModifierKey::Super);
	}
	return event;
}
#else
//------------------------------------------------------------------------
static bool translateKeyMessage (VstKeyCode& keyCode, char16 key, int16 keyMsg, int16 modifiers)
{
	keyCode.character = 0;
	keyCode.virt = (unsigned char)keyMsg;
	keyCode.modifier = 0;
	if (key == 0)
		key = VirtualKeyCodeToChar ((uint8)keyMsg);
	if (key)
	{
		String keyStr (STR (" "));
		keyStr.setChar16 (0, key);
		keyStr.toMultiByte (kCP_Utf8);
		if (keyStr.length () == 1)
			keyCode.character = keyStr.getChar8 (0);
	}
	if (modifiers)
	{
		if (modifiers & kShiftKey)
			keyCode.modifier |= MODIFIER_SHIFT;
		if (modifiers & kAlternateKey)
			keyCode.modifier |= MODIFIER_ALTERNATE;
		if (modifiers & kCommandKey)
			keyCode.modifier |= MODIFIER_CONTROL;
		if (modifiers & kControlKey)
			keyCode.modifier |= MODIFIER_COMMAND;
	}
	return true;
}
#endif

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::onKeyDown (char16 key, int16 keyMsg, int16 modifiers)
{
	if (frame)
	{
#if VSTGUI_NEWER_THAN_4_10
		auto event = translateKeyMessage (key, keyMsg, modifiers);
		event.type = EventType::KeyDown;
		frame->dispatchEvent (event);
		if (event.consumed)
			return kResultTrue;
#else
		VstKeyCode keyCode = {};
		if (translateKeyMessage (keyCode, key, keyMsg, modifiers))
		{
			VSTGUI_INT32 result = frame->onKeyDown (keyCode);
			if (result == 1)
				return kResultTrue;
		}
#endif
	}
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::onKeyUp (char16 key, int16 keyMsg, int16 modifiers)
{
	if (frame)
	{
#if VSTGUI_NEWER_THAN_4_10
		auto event = translateKeyMessage (key, keyMsg, modifiers);
		event.type = EventType::KeyUp;
		frame->dispatchEvent (event);
		if (event.consumed)
			return kResultTrue;
#else
		VstKeyCode keyCode = {};
		if (translateKeyMessage (keyCode, key, keyMsg, modifiers))
		{
			VSTGUI_INT32 result = frame->onKeyUp (keyCode);
			if (result == 1)
				return kResultTrue;
		}
#endif
	}
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::onWheel (float distance)
{
	if (frame)
	{
#if VSTGUI_NEWER_THAN_4_10
		CPoint where;
		frame->getCurrentMouseLocation (where);

		MouseWheelEvent event;
		event.mousePosition = where;
		event.deltaY = distance;

		frame->getPlatformFrame ()->getCurrentModifiers (event.modifiers);
		frame->dispatchEvent (event);
		if (event.consumed)
			return kResultTrue;
#else
		CPoint where;
		frame->getCurrentMouseLocation (where);
		if (frame->onWheel (where, kMouseWheelAxisY, distance, frame->getCurrentMouseButtons ()))
			return kResultTrue;
#endif
	}
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::setFrame (IPlugFrame* frame)
{
#if 0
	if (frame)
	{
		FUnknownPtr<IPlugFrameIdle> frameIdle (frame);
		if (frameIdle)
			frameIdle->addIdleHandler (this);

	}
	else if (plugFrame)
	{
		FUnknownPtr<IPlugFrameIdle> frameIdle (plugFrame);
		if (frameIdle)
			frameIdle->removeIdleHandler (this);
	}
#endif
	return EditorView::setFrame (frame);
}

//------------------------------------------------------------------------
} // namespace Vst
} // namespace Steinberg
