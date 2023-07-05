//-----------------------------------------------------------------------------
// Flags       : clang-format auto
// Project     : VST SDK
//
// Category    : EditorHost
// Filename    : public.sdk/samples/vst-hosting/editorhost/source/platform/mac/window.mm
// Created by  : Steinberg 09.2016
// Description : Example of opening a plug-in editor
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

#import "public.sdk/samples/vst-hosting/editorhost/source/platform/mac/window.h"

#import <Cocoa/Cocoa.h>

#if !__has_feature(objc_arc)
#error this file needs to be compiled with automatic reference counting enabled
#endif

using namespace Steinberg::Vst;

//------------------------------------------------------------------------
@interface VSTSDK_WindowDelegate : NSObject <NSWindowDelegate>
#if __i386__
{
	std::shared_ptr<EditorHost::Window> _window;
	NSWindow* _nsWindow;
}
#endif
@property (readonly) std::shared_ptr<EditorHost::Window> window;
@property (strong, readwrite) NSWindow* nsWindow;

- (id)initWithWindow:(std::shared_ptr<EditorHost::Window>)window;
@end

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {
namespace EditorHost {

//------------------------------------------------------------------------
struct Window::Impl
{
	WindowControllerPtr controller;
	VSTSDK_WindowDelegate* nsWindowDelegate {nullptr};
};

//------------------------------------------------------------------------
WindowPtr Window::make (const std::string& name, Size size, bool resizeable,
                        const WindowControllerPtr& controller)
{
	auto window = std::make_shared<Window> ();
	if (window->init (name, size, resizeable, controller))
		return window;
	return nullptr;
}

//------------------------------------------------------------------------
Window::Window ()
{
	impl = std::unique_ptr<Impl> (new Impl);
}

//------------------------------------------------------------------------
bool Window::init (const std::string& name, Size size, bool resizeable,
                   const WindowControllerPtr& controller)
{
	impl->controller = controller;
	NSUInteger styleMask = NSTitledWindowMask | NSClosableWindowMask;
	if (resizeable)
		styleMask |= NSResizableWindowMask;
	auto contentRect =
	    NSMakeRect (0., 0., static_cast<CGFloat> (size.width), static_cast<CGFloat> (size.height));
	impl->nsWindowDelegate = [[VSTSDK_WindowDelegate alloc] initWithWindow:shared_from_this ()];
	auto nsWindow = [[NSWindow alloc] initWithContentRect:contentRect
	                                            styleMask:styleMask
	                                              backing:NSBackingStoreBuffered
	                                                defer:YES];
	[nsWindow setDelegate:impl->nsWindowDelegate];
	nsWindow.releasedWhenClosed = NO;
	impl->nsWindowDelegate.nsWindow = nsWindow;
	[nsWindow center];
	return true;
}

//------------------------------------------------------------------------
Window::~Window () noexcept
{
}

//------------------------------------------------------------------------
void Window::show ()
{
	auto nsWindow = impl->nsWindowDelegate.nsWindow;
	impl->controller->onShow (*this);
	[nsWindow makeKeyAndOrderFront:nil];
}

//------------------------------------------------------------------------
void Window::close ()
{
	auto nsWindow = impl->nsWindowDelegate.nsWindow;
	[nsWindow close];
}

//------------------------------------------------------------------------
void Window::resize (Size newSize)
{
	auto nsWindow = impl->nsWindowDelegate.nsWindow;
	auto r = [nsWindow contentRectForFrameRect:nsWindow.frame];
	auto diff = newSize.height - r.size.height;
	r.size.width = newSize.width;
	r.size.height = newSize.height;
	r.origin.y -= diff;
	[nsWindow setFrame:[nsWindow frameRectForContentRect:r]
	           display:[nsWindow isVisible]
	           animate:NO];
}

//------------------------------------------------------------------------
Size Window::getContentSize ()
{
	auto nsWindow = impl->nsWindowDelegate.nsWindow;
	auto r = [nsWindow contentRectForFrameRect:nsWindow.frame];
	return {static_cast<Coord> (r.size.width), static_cast<Coord> (r.size.height)};
}

//------------------------------------------------------------------------
NativePlatformWindow Window::getNativePlatformWindow () const
{
	auto nsWindow = impl->nsWindowDelegate.nsWindow;
	return {kPlatformTypeNSView, (__bridge void*)([nsWindow contentView])};
}

//------------------------------------------------------------------------
WindowControllerPtr Window::getController () const
{
	return impl->controller;
}

//------------------------------------------------------------------------
void Window::windowClosed ()
{
	impl->controller->onClose (*this);
	impl->nsWindowDelegate = nullptr;
}

//------------------------------------------------------------------------
} // EditorHost
} // Vst
} // Steinberg

//------------------------------------------------------------------------
@implementation VSTSDK_WindowDelegate
#if __i386__
@synthesize window = _window;
#endif

//------------------------------------------------------------------------
- (id)initWithWindow:(std::shared_ptr<EditorHost::Window>)window
{
	if (self = [super init])
	{
		self->_window = window;
	}
	return self;
}

//------------------------------------------------------------------------
- (NSSize)windowWillResize:(nonnull NSWindow*)sender toSize:(NSSize)frameSize
{
	NSRect r {};
	r.size = frameSize;
	r = [sender contentRectForFrameRect:r];
	EditorHost::Size size {static_cast<EditorHost::Coord> (r.size.width),
	                       static_cast<EditorHost::Coord> (r.size.height)};
	size = self.window->getController ()->constrainSize (*self.window, size);
	r.size.width = size.width;
	r.size.height = size.height;
	r = [sender frameRectForContentRect:r];
	return r.size;
}

//------------------------------------------------------------------------
- (void)windowDidResize:(nonnull NSNotification*)notification
{
	NSWindow* window = [notification object];
	NSRect r = window.frame;
	r = [window contentRectForFrameRect:r];
	EditorHost::Size size {static_cast<EditorHost::Coord> (r.size.width),
	                       static_cast<EditorHost::Coord> (r.size.height)};
	self.window->getController ()->onResize (*self.window, size);
}

//------------------------------------------------------------------------
- (void)windowWillClose:(nonnull NSNotification*)notification
{
	self.window->windowClosed ();
	self->_window = nullptr;
	self->_nsWindow = nullptr;
}

@end
