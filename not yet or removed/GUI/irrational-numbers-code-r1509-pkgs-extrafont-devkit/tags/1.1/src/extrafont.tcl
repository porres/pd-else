## extrafont.tcl -- a multi-platform binary package for loading "private fonts"

## Copyright (c) 2017 by A.Buratti
##
## This library is free software; you can use, modify, and redistribute it
## for any purpose, provided that existing copyright notices are retained
## in all copies and that this notice is included verbatim in any
## distributions.
##
## This software is distributed WITHOUT ANY WARRANTY; without even the
## implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
##

namespace eval extrafont {
	variable _ExtraFonts {}
	variable _TempFile   ;# array
	variable _TempDir
	
	set _TempDir ""
		
	proc _isVfsFile {filename} {
		expr { [lindex [file system $filename] 0] != "native" }
	}
	
	 # when Tk is destroyed (e.g on exit), then do a cleanup
	trace add command "." delete {apply { {args} {extrafont::cleanup} } }
}

proc extrafont::_copyToTempFile {filename} {
	variable _TempDir

	if { $_TempDir == "" } {
		set _TempDir [futmp::mktempdir [futmp::tempdir] extrafont_]		
		 # don't catch error; let it raise
	}

	set fd [open $filename r] ;# on error let it raise ..
	fconfigure $fd -translation binary
		
	 # note: tempfile returns an open channel ; the filename is returned via upvar (in newfilename var)
	set newfilename ""
	set wentWrong [catch {
		set cacheChannel [futmp::tempfile newfilename $_TempDir cache_ [file extension $filename]]
		fconfigure $cacheChannel -translation binary
		} errmsg ]
	if { $wentWrong } {
		close $fd
		error $errmsg
	}

	set wentWrong [catch {
		fcopy $fd $cacheChannel
		} errmsg ]

	close $cacheChannel
	close $fd
	
	if { $wentWrong } {
		error $errmsg
	}

	return $newfilename
}

proc extrafont::load {fontfile} {
	variable _ExtraFonts
	variable _TempFile
			
	set fontfile [file normalize $fontfile]
	if { $fontfile in $_ExtraFonts } {
		error "Fontfile \"$fontfile\" already loaded."
	}
	set orig_fontfile $fontfile
	if { [_isVfsFile $fontfile] } {
		set orig_fontfile $fontfile

		set fontfile [_copyToTempFile $orig_fontfile] ;# on error let it raise ..

		set _TempFile($orig_fontfile) $fontfile	
	}
	if { [catch {core::load $fontfile} errmsg] } {
		if { [info exists _TempFile($orig_fontfile)] } {
			unset _TempFile($orig_fontfile)
		}	
		error [string map [list $fontfile $orig_fontfile] $errmsg]	
	}
	lappend _ExtraFonts $orig_fontfile
	return
} 

 # Be careful: since this proc could be called when app exits,
 #  you cannot rely on other packages (e.g. vfs ), since they could have been destroyed before.
 # Therefore DO NOT use code from other packages
proc extrafont::unload {fontfile} {
	variable _ExtraFonts
	variable _TempFile
	
	set fontfile [file normalize $fontfile]

	 # Fix for MacOSX : 
	 # Since core::unload does not return an error when unloading an not-loaded file,
	 # we must check-it before
	if { $::tcl_platform(os) == "Darwin" } {
		if { $fontfile ni $_ExtraFonts } {
			error "error 0 - cannot unload font \"$fontfile\""
		}
	}
	set orig_fontfile $fontfile
	set isVfs [info exists _TempFile($orig_fontfile)]
	if { $isVfs } {
		set fontfile  $_TempFile($orig_fontfile)
	}
	if { [catch {core::unload $fontfile} errmsg] } {
		error [string map [list $fontfile $orig_fontfile] $errmsg]	
	}

	if { $isVfs } {
		catch {file delete $fontfile}  ;# skip errors ..
		unset _TempFile($orig_fontfile)
	}	
	set idx [lsearch $_ExtraFonts $orig_fontfile]
	if { $idx != -1 } {		
		set _ExtraFonts [lreplace $_ExtraFonts $idx $idx]
	}
	return	
} 

# -- some utilities

proc extrafont::loaded {} {
	variable _ExtraFonts

	return $_ExtraFonts
}

 # remove all the loaded extrafonts (with all the underlying OS stuff at OS level)  
proc extrafont::cleanup {} {
	variable _ExtraFonts
	variable _TempDir

	foreach fontfile $_ExtraFonts {
		catch {unload $fontfile}  ;# don't stop it now !
	}
	 # now _ExtraFonts should be an empty list ; anyway since this is a STRONG cleanup, force it !
	set _ExtraFonts {}
	if { $_TempDir != "" } {
		file delete -force $_TempDir ;# brute force
		set _TempDir ""
	}
	# nothing required on the core side
}

 # WARNING; on MacOSX after loading/unloading one or more fonts, the list
 # of the availables fonts (i.e. [font families]) won't be updated till the next event-loop update.
 # For this reason, if your script needs to call isAvalable/availableFamilies
 # just after loading/unloading a fontfile, you need to call the "update" command. 
proc extrafont::isAvailable {family} {
	expr [lsearch -nocase -exact [font families] $family] == -1 ? false : true
}

proc extrafont::availableFamilies { {familyPattern {*}} } {
	lsearch -all -inline -glob -nocase [font families] $familyPattern
}
