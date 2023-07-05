//------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
//
// Project     : Steinberg Plug-In SDK
// Filename    : public.sdk/source/common/systemclipboard_win32.cpp
// Created by  : Steinberg 04.2020
// Description : Simple helper allowing to copy/retrieve text to/from the system clipboard
//
//-----------------------------------------------------------------------------
// LICENSE
// (c) 2023, Steinberg Media Technologies GmbH, All Rights Reserved
//-----------------------------------------------------------------------------
// This Software Development Kit may not be distributed in parts or its entirety
// without prior written agreement by Steinberg Media Technologies GmbH.
// This SDK must not be used to re-engineer or manipulate any technology used
// in any Steinberg or Third-party application or software module,
// unless permitted by law.
// Neither the name of the Steinberg Media Technologies nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// THIS SDK IS PROVIDED BY STEINBERG MEDIA TECHNOLOGIES GMBH "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL STEINBERG MEDIA TECHNOLOGIES GMBH BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#include "systemclipboard.h"
#include "pluginterfaces/base/fplatform.h"

#if SMTG_OS_WINDOWS
#include <vector>
#include <windows.h>

//------------------------------------------------------------------------
namespace Steinberg {
namespace SystemClipboard {
namespace {

//------------------------------------------------------------------------
struct Clipboard
{
	Clipboard () { open = OpenClipboard (nullptr); }
	~Clipboard ()
	{
		if (open)
			CloseClipboard ();
	}

	bool open {false};
};

//------------------------------------------------------------------------
std::vector<WCHAR> convertToWide (const std::string& text)
{
	std::vector<WCHAR> wideStr;

	auto numChars =
	    MultiByteToWideChar (CP_UTF8, 0, text.data (), static_cast<int> (text.size ()), nullptr, 0);
	if (numChars)
	{
		wideStr.resize (static_cast<size_t> (numChars) + 1);
		numChars = MultiByteToWideChar (CP_UTF8, 0, text.data (), static_cast<int> (text.size ()),
		                                wideStr.data (), static_cast<int> (wideStr.size ()));
	}
	wideStr[numChars] = 0;
	wideStr.resize (static_cast<size_t> (numChars) + 1);
	return wideStr;
}

//------------------------------------------------------------------------
std::string convertToUTF8 (const WCHAR* data, const SIZE_T& dataSize)
{
	std::string text;
	auto numChars = WideCharToMultiByte (CP_UTF8, 0, data, dataSize / sizeof (WCHAR), nullptr, 0,
	                                     nullptr, nullptr);
	text.resize (static_cast<size_t> (numChars) + 1);
	numChars = WideCharToMultiByte (CP_UTF8, 0, data, dataSize / sizeof (WCHAR),
	                                const_cast<char*> (text.data ()),
	                                static_cast<int> (text.size ()), nullptr, nullptr);
	text.resize (numChars);
	return text;
}

//------------------------------------------------------------------------
} // anonymous

//-----------------------------------------------------------------------------
bool copyTextToClipboard (const std::string& text)
{
	Clipboard cb;
	if (text.empty () || !cb.open)
		return false;

	if (!EmptyClipboard ())
		return false;

	bool result = false;

	auto wideStr = convertToWide (text);

	auto byteSize = wideStr.size () * sizeof (WCHAR);

	if (auto memory = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, byteSize))
	{
		if (auto* data = static_cast<WCHAR*> (GlobalLock (memory)))
		{
#if defined(__MINGW32__)
			memcpy (data, wideStr.data (), byteSize);
#else
			memcpy_s (data, byteSize, wideStr.data (), byteSize);
#endif
			GlobalUnlock (memory);

			auto handle = SetClipboardData (CF_UNICODETEXT, memory);
			result = handle != nullptr;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
bool getTextFromClipboard (std::string& text)
{
	Clipboard cb;
	if (!cb.open)
		return false;

	if (!IsClipboardFormatAvailable (CF_UNICODETEXT))
		return false;

	bool result = false;

	// Get handle of clipboard object for unicode text
	if (auto hData = GetClipboardData (CF_UNICODETEXT))
	{
		// Lock the handle to get the actual text pointer
		if (auto* data = (const WCHAR*)GlobalLock (hData))
		{
			auto dataSize = GlobalSize (hData);
			text = convertToUTF8 (data, dataSize);

			// Release the lock
			GlobalUnlock (hData);

			result = true;
		}
	}

	return result;
}

//------------------------------------------------------------------------
} // namespace SystemClipboard
} // namespace Steinberg

#endif // SMTG_OS_WINDOWS
