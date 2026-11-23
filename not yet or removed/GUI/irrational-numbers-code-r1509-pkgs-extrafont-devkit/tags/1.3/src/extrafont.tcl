## extrafont.tcl -- a multi-platform binary package for loading "private fonts"

## Copyright (c) 2017,2018 by A.Buratti
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
	 # FFFD-table is the core data structure holding all the relations between
	 #  font-files, font-familiies, font-fullnames and font-details.
	 # *** NOTE: we are talking just about fonts loaded with extrafont::load;  
	 # ***      fonts loaded at system-level are not included here.
	 #  They first three components FFF (font-file, font-familiy, font-fullname)
	 #  gives you a primary key for the D component (the font-detail dictionary)
	 #  
	variable _FFFD_Table    ;# array: key is (file,family,fullname)
							;#        value is the font-detail
	variable _File2TempFile ;# array: key is the originale filename (normalized),
	                        ;#        value is its temporary working copy
	variable _TempDir
	
	array unset _FFFD_Table
	array unset _File2TempFile
	set _TempDir ""
		
	proc _isVfsFile {filename} {
		expr { [lindex [file system $filename] 0] != "native" }
	}

	 # load thi submodule" fontnameinfo.tcl" in a sub-namespace
	 # It provides the 'nameinfo' command
	namespace eval nametable {
		source [file join [file dirname [info script]] fontnameinfo.tcl]
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

 # extrafont::load fontfile
 # ------------------------
 # install the fonts contained in $fontfile and return a list of  font-families 
 # eg:   "{ "Family A"  "Family B"  ...  }
 # Usually the returned list holds just one font-family.
 # If you load an OpenTypeCollections (*.ttc), it may contains more than one font;
 # usuallly this fonts are variants of the same font-family, but nothing prevents a *.ttc
 # to include different font-families. Note that the returned list
 # may contain duplicates. 
 #
 # In order to discover the differences for these fants of the same font-family,
 # use  extrafont::query ...   for extracting the font-fullnames, or the full details.
proc extrafont::load {fontfile} {
	variable _FFFD_Table
	variable _File2TempFile
	
	set fontfile [file normalize $fontfile]
	set orig_fontfile $fontfile
	if { [array names _FFFD_Table $orig_fontfile,*] != {} } {
		error "Fontfile \"$orig_fontfile\" already loaded."	
	}
	
	if { [_isVfsFile $orig_fontfile] } {
		set fontfile [_copyToTempFile $orig_fontfile] ;# on error let it raise 
		set _File2TempFile($orig_fontfile)   $fontfile	
	}
	if { [catch {core::load $fontfile} errmsg] } {
		array unset _File2TempFile $orig_fontfile
		error [string map [list $fontfile $orig_fontfile] $errmsg]
	}
	set fontsInfo {}
	 # if nameinfo fails, don't stop; return an empty list
	catch {
		set fontsInfo [nametable::nameinfo $fontfile]
	}
	set FamList {}
	foreach fontInfo $fontsInfo {
		set family [dict get $fontInfo "fontFamily"]
		set fullname [dict get $fontInfo "fullName"]
		set _FFFD_Table($orig_fontfile,$family,$fullname) $fontInfo
		lappend FamList $family
	}
	# ? should I return preferredFamily ?
	return $FamList
} 


 # extrafont::unload fontfile
 # --------------------------
 # Be careful: since this proc could be called when app exits,
 #  you cannot rely on other packages (e.g. vfs ), since they could have been destroyed before.
 # Therefore DO NOT use within this proc code from other packages
proc extrafont::unload {fontfile} {
	variable _FFFD_Table
	variable _File2TempFile
	
	set fontfile [file normalize $fontfile]
	
	 # Fix for MacOSX : 
	 # Since core::unload does not return an error when unloading a not-loaded file,
	 # we must check-it before
	if { $::tcl_platform(os) == "Darwin" } {
		if { [query files -file $fontfile]  == {}  } {
			error "error 0 - cannot unload font \"$fontfile\""
		}
	}
	set orig_fontfile $fontfile
	set isVfs [info exists _File2TempFile($orig_fontfile)]
	if { $isVfs } {
		set fontfile  $_File2TempFile($orig_fontfile)
	}
	if { [catch {core::unload $fontfile} errmsg] } {
		error [string map [list $fontfile $orig_fontfile] $errmsg]
	}
	
	if { $isVfs } {
		catch {file delete $fontfile}  ;# skip errors
		unset _File2TempFile($orig_fontfile)
	}
	array unset _FFFD_Table $fontfile,*
	return	
} 


 # extrafont::loaded
 #  returns the list of the currently loaded (extra)font-files.
 #
 # OBSOLETE
 # extrafont::loaded has been obsoleted by the extrafont::query  command
 # and it is currently suported just for backward compatibility.
 #
 # extrafont::loaded
 #    is equivalent to
 # extrafont::query files
proc extrafont::loaded {} {
	variable _FFFD_Table
	return [query files]
}


 # extrafont::query _kind_ ? _selector_ _pattern_ ?
 # ----------------------------------------------
 # returns list of *extrafont-loaded*  files,families,fullnames,details
 # matching  -file,-family,-fullname   pattern
 # NOTE:  system-installed fonts are not excluded; this query deals with
 #   extrafonts-installed fonts only.
 # Example:
 #  query files
 #  query files -file   Ariel*.ttf
 #  query files -family Ariel*
 #  query families
 #  query families  -file  Ariel*.ttf
 #  query fullnames
 #  query fullnames -family Ariel*
 #  ...
 #  query details  -family Ariel*
proc extrafont::query { kind args } {
	variable _FFFD_Table
	
	set allowedValues {files families fullnames details}
	if { $kind ni $allowedValues } {
		error "bad kind \"$kind\": must be [join $allowedValues ","]"
	}
	
	if { $args == {} } {
		set selector "(empty)"  ;# dummy selector
	} elseif { [llength $args] == 2 } {
		lassign $args selector selectorVal
		set allowedValues {-file -family -fullname}
		if { $selector ni $allowedValues } {
			error "bad selector \"$selector\": must be [join $allowedValues ","]"
		}
	} else {
		error "wrong params: query _kind_ ?selector value?"
	}
	
	switch -- $selector {
		(empty)		{ set pattern "*" }
		-file		{ set pattern "$selectorVal,*,*" }
		-family		{ set pattern "*,$selectorVal,*"	}
		-fullname	{ set pattern "*,*,$selectorVal" }
	}
	
	set L {}
	foreach { key detail } [array get _FFFD_Table $pattern] {
		lassign [split $key ","] fontfile family fullname 
		switch -- $kind {
			files	{ lappend L $fontfile }
			families {	lappend L $family }
			fullnames { lappend L $fullname}
			details {lappend L $detail }
		} 
	}
	lsort -unique $L 
}


 # nameinfo $fontfile ?$fontPlatformID?
 # ------------------
 # Returns a list of font-info. One font-info (a dictionary) for each font
 # contained in $fontfile.
 #
 # fontPlatformName can be used for selecting data tailored for a given platform.
 # It can be ("" (default) "win" or "mac"  - no support for the "Unicode platform").
 # If fontPlatformName is "" then the extracted info are those required for
 # the current platform (i.e "win" for windows and "mac" for mac/linux/../restOfTheWorld) 
 #
 # Implementation note:
 #  if $fontfile is loaded, then the 'cached' font-infos are returned,
 #  else these are extracted by calling [nametable::nameinfo $fontfile]
proc extrafont::nameinfo {fontfile {fontPlatformName ""}} {
	variable _FFFD_Table
	
	if { $fontPlatformName eq "" } {
		if { $::tcl_platform(platform) == "windows" } {
			set fontPlatformName "win"
		} else {
			set fontPlatformName "mac"
		}
	}
	switch -- $fontPlatformName {
		"mac" { set fontPlatformID 1 }
		"win" { set fontPlatformID 3 }
		default { error "invalid fontPlatformName. Valid values are win, mac."}
	}
	
	set fontfile [file normalize $fontfile]
	set res [query details -file $fontfile]
	if { $res == {} } {
		set res [nametable::nameinfo $fontfile $fontPlatformID]
	}
	return $res
}

 # extrafont::cleanup
 # ------------------
 # remove all the loaded extrafonts (with all the underlying OS stuff at OS level)  
proc extrafont::cleanup {} {
	variable _FFFD_Table
	variable _File2TempFile
	variable _TempDir
	
	foreach fontfile [query files] {
		catch {unload $fontfile}  ;# don't stop it now !
	}
	
	if { $_TempDir != "" } {
		file delete -force $_TempDir ;# brute force
		set _TempDir ""
	}
	# nothing required on the core side
	return
}


 # extrafont::isAvailable $family
 # ------------------------------
 # test if a given font-family is available.
 # WARNING; on MacOSX after loading/unloading one or more fonts, the list
 # of the availables fonts (i.e. [font families]) won't be updated till the next event-loop update.
 # For this reason, if your script needs to call isAvalable/availableFamilies
 # just after loading/unloading a fontfile, you need to call the "update" command. 
proc extrafont::isAvailable {family} {
	expr [lsearch -nocase -exact [font families] $family] == -1 ? false : true
}


 # extrafont::availableFamilies ?pattern?
 # --------------------------------------
 # returns the list of available fontfamiles matching pattern.
 # NOTE:
 #   extrafont::availableFamilies   and     extrafont::query families
 #   are quit similar, and they bot returns a list of matching font-families.
 #   They key difference is that
 #    extrafont::query families  -families A*
 #   matches the loaded extra-fonts ONLY
 #   whilst
 #    extrafont::avalableFamilies A*
 #   matches all the loaded font-families (both system-wide fonts and private-fonts)
 #   (and it's a case-sensitive matching)
 #   
proc extrafont::availableFamilies { {familyPattern {*}} } {
	lsearch -all -inline -glob -nocase [font families] $familyPattern
}
