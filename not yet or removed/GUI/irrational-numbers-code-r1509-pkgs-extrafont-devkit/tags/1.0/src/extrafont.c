/* 
  ==========================================================================
  TclTk extension (Win/Linux/MacOSX):
    extrafont::load
  This command makes available to TclTk apps a new font without intalling it.
   ---
  July 2017 - A.Buratti fecit
  ==========================================================================
*/

#include <tcl.h>

#if defined(__WIN32__)
  #include <Windows.h>
  #include <Wingdi.h>
#elif defined(__linux__)
  #include <fontconfig/fontconfig.h>
#elif defined(__APPLE__)
  #include <CoreFoundation/CoreFoundation.h>
  #include <CoreText/CTFontManager.h>
#endif

 // This macros is a short for the standard parameter-list of Tcl_ObjCmdProc conformant functions.
 // Note that the first parameter (ClientData, i.e (void*)) is missing, since it should be
 // explicitely defined as a specific pointer.
#define OTHER_CMDPROC_ARGS   Tcl_Interp *_interp, int _objc, Tcl_Obj* const _objv[]


static
int CMDPROC_loadfont( ClientData unused, OTHER_CMDPROC_ARGS ) {
	int r = TCL_OK;
    if (_objc != 2) {
        Tcl_WrongNumArgs(_interp, 1, _objv, "filename");
        return TCL_ERROR;
    }
	
	int len;
	const char *path = Tcl_GetStringFromObj(_objv[1], &len);
	int res;
	
#if defined(__WIN32__)
	Tcl_DString ds;
	Tcl_Encoding unicode;
	
	Tcl_DStringInit(&ds);
	unicode = Tcl_GetEncoding(_interp, "unicode");
	Tcl_UtfToExternalDString(unicode, path, len, &ds);
	res = AddFontResourceExW((LPCWSTR)Tcl_DStringValue(&ds), FR_PRIVATE, NULL);
	Tcl_DStringFree(&ds);
	Tcl_FreeEncoding(unicode);	
	
#elif defined(__linux__)
	res = FcConfigAppFontAddFile(NULL,path);

#elif defined(__APPLE__)
	CFStringRef filenameStr = CFStringCreateWithCString( NULL, path, kCFStringEncodingUTF8 );
	CFURLRef fileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
	                           filenameStr,      // file path name
	                           kCFURLPOSIXPathStyle,   // interpret as POSIX path
	                           false);                 // is it a directory?
	CFRelease(filenameStr);
	CFErrorRef error = nil;
	res = CTFontManagerRegisterFontsForURL(fileURL,kCTFontManagerScopeProcess,&error);
	
	CFRelease(fileURL);

#endif

	if ( ! res ) {
		Tcl_SetObjResult( _interp, Tcl_ObjPrintf("error %d - cannot load font %s", res, path) );
	    r = TCL_ERROR;
	}
	return r;	
}

 
DLLEXPORT
int Extrafont_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.5", 0) == NULL) {
        return TCL_ERROR;
    }
	
	if ( ! Tcl_CreateObjCommand(interp, "extrafont::load", 
		CMDPROC_loadfont,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL) ) {
		return TCL_ERROR;
	}
		   
	return TCL_OK;
}
