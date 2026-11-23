/* 
  ==========================================================================
  TclTk extension (Win/Linux/MacOSX):
    extrafont::load
  This command makes available to TclTk apps a new font without isntalling it.
   ---
  July 2017 - A.Buratti fecit
  May  2018 - BugFix for MY_FcConfigAppFontRemoveFile (linux only)
  ==========================================================================
*/

#include <tcl.h>

#if defined(__WIN32__)
  #include <Windows.h>
  #include <Wingdi.h>
#elif defined(__linux__)
  #include <stdlib.h>
  #include <string.h>
  #include <fontconfig/fontconfig.h>
#elif defined(__APPLE__)
  #include <CoreFoundation/CoreFoundation.h>
  #include <CoreText/CTFontManager.h>
#endif

 // This macro is a short for the standard parameter-list of Tcl_ObjCmdProc conformant functions.
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
	CFURLRef fileURL = CFURLCreateWithFileSystemPath(NULL, filenameStr, kCFURLPOSIXPathStyle, false);

	CFRelease(filenameStr);
	CFErrorRef error = nil;
	res = CTFontManagerRegisterFontsForURL(fileURL,kCTFontManagerScopeProcess,&error);
	
	CFRelease(fileURL);

#endif

	if ( ! res ) {
		Tcl_SetObjResult( _interp, Tcl_ObjPrintf("error %d - cannot load font \"%s\"", res, path) );
	    r = TCL_ERROR;
	}
	return r;	
}



#if defined(__linux__)
/*
 * The inverse of FcConfigAppFontAddFile().
 * - Currently (in Fontconfig 2.12.4) this function is missing,
 * so, here is my hack.
 * WARNING: based on internal code hacking, not documented/supported.
 */

static
FcBool MY_FcConfigAppFontRemoveFile( FcConfig *config, const FcChar8  *filename) {
	FcFontSet *set = FcConfigGetFonts(config,FcSetApplication);
	if ( !set ) return FcFalse;

	FcBool res = FcFalse;
	int found = 0;
	  // scan the set->fonts array, and copy all the elements but those with matching 'filename'.
	  // Finally substitute the new (reduced) array to set->fonts.
	if ( set->nfont == 0 ) return FcFalse;

	FcPattern **newFonts = (FcPattern **)calloc( set->sfont, sizeof(FcPattern *));
	FcPattern **newP = newFonts;
	for ( int i = 0 ; i < set->nfont ; ++i ) {
		FcChar8 *strRef = NULL;
		FcPatternGetString( set->fonts[i], FC_FILE, 0, &strRef);
		if( strRef &&  strcmp(strRef,filename)==0 ) {
			++found;
			FcPatternDestroy(set->fonts[i]);
		} else {
			*newP++ = set->fonts[i];
		}
	}
	if( found > 0 ) {
		 //free just the array of pointers; don't free the pointed objects (they have been copied in newFonts)
		free(set->fonts);
		 // set newFonts as the new set->fonts
		set->fonts = newFonts;
		set->nfont -= found;
		res = FcTrue;
	} else {
		free(newFonts);  // free just the array 
		res = FcFalse;
	}
	return res;
}
#endif


static
int CMDPROC_unloadfont( ClientData unused, OTHER_CMDPROC_ARGS ) {
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
	res = RemoveFontResourceExW( (LPCWSTR)Tcl_DStringValue(&ds), FR_PRIVATE, NULL );
	Tcl_DStringFree(&ds);
	Tcl_FreeEncoding(unicode);	
	
#elif defined(__linux__)
	 // Given that Fontconfig (2.12.4) lacks of the inverse of FcConfigAppFontAddFile
	 // we use a custom hacked function.
	res = MY_FcConfigAppFontRemoveFile( NULL, path ) ? 1 : 0 ;

#elif defined(__APPLE__)
	CFStringRef filenameStr = CFStringCreateWithCString( NULL, path, kCFStringEncodingUTF8 );
	CFURLRef fileURL = CFURLCreateWithFileSystemPath(NULL, filenameStr, kCFURLPOSIXPathStyle, false);

	CFRelease(filenameStr);
	CFErrorRef error = nil;
	res = CTFontManagerUnregisterFontsForURL(fileURL,kCTFontManagerScopeProcess,&error);
	
	CFRelease(fileURL);

#endif

	if ( ! res ) {
		Tcl_SetObjResult( _interp, Tcl_ObjPrintf("error %d - cannot unload font \"%s\"", res, path) );
	    r = TCL_ERROR;
	}
	return r;	
}


 
DLLEXPORT
int Extrafont_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.5", 0) == NULL) {
        return TCL_ERROR;
    }
	
	if ( ! Tcl_CreateObjCommand(interp, "extrafont::core::load", 
		CMDPROC_loadfont,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL) ) {
		return TCL_ERROR;
	}
	if ( ! Tcl_CreateObjCommand(interp, "extrafont::core::unload", 
		CMDPROC_unloadfont,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL) ) {
		return TCL_ERROR;
	}
		   
	return TCL_OK;
}
