# fontnameinfo.tcl
#
# Commands for extracting details from the "name' table of OpenType font-files.
#
# Tested with 
#     *.otf  (including those with PostScript outlines (OTTO))
#     *.ttf
#     *.ttc  (OpenTypeCollections)
#
# Reference Specification:
#     Opentype 1.6 - http://www.microsoft.com/typography/otspec/
#
# This module derives from the module Glyph
#  ( see http://wiki.tcl.tk/37854 )
# which in turn is inspired by the following works:
#   * org.apache.batik.svggen project  (Apache License, 2.0)
#   * pdf4tcl project
#     Copyright (c) 2004 by Frank Richter <frichter@truckle.in-chemnitz.de> and
#                       Jens Ponisch <jens@ruessel.in-chemnitz.de>
#     Copyright (c) 2006-2012 by Peter Spjuth <peter.spjuth@gmail.com>
#     Copyright (c) 2009 by Yaroslav Schekin <ladayaroslav@yandex.ru>
#   * sfntutil.tcl - by Lars Hellstrom


	#  NameIDs for the name table.
set _NameID2Str [dict create {*}{
	0  copyright
	1  fontFamily
	2  fontSubfamily
	3  uniqueID
	4  fullName
	5  version
	6  postScriptName
	7  trademark
	8  manufacturer
	9  designer
	10 description
	11 manufacturerURL
	12 designerURL
	13 license
	14 licenseURL
	15 reserved
	16 typographicFamily
	17 typographicSubfamily
	18 compatibleFullName
	19 sampleText
	20 postScriptFindFontName
	21 wwsFamily
	22 wwsSubfamily
	23 lightBackgroundPalette
	24 darkBackgroundPalette
	25 variationsPostScriptNamePrefix
}]

	# return all the valid keys for the font-info dictionary
	# NOTE: none of these nameID is mandatory, but the following
	#       are strongly recommended:
	#   1 fontFamily
	#   2 fontSubfamily
	#   4 fullName
	#  ? 16 typographicFamily
	#  ? 17 typographicSubfamily
	#
	#   Reference:
	#   https://docs.microsoft.com/en-us/typography/opentype/spec/name#name-ids
	#
	# Note: currently extrafont requires just the following mandatory nameID
	#    fontFamily
	#    fullName
proc nameIDs {} {
	variable _NameID2Str
	dict values $_NameID2Str 
}


 # nameinfo $fontPath
 # ------------------
 # scan the 'name' table(s) of $fontPath, and returns a list of font-info
 # One font-info for each name table
 # Each font-info is a dictionary 
 # (see [nameIDS] for the keys; not all the keys are mandatory)
 #
 # fontPlatformID can be ("" (default) 3 (windows) 1 (everything but windows ).
 # If fontPlatformID is "" then the extracted info are those required for
 # the current platform (i.e 3 for "windows" or 1 for mac/linux/...) 
 # 
 # An error is raised if fontPath cannot be properly parsed.
proc nameinfo {fontPath {fontPlatformID ""}} {
	set fd [open $fontPath "r"]
	fconfigure $fd -translation binary
		set failed [catch {set names [_ReadFontFile $fd $fontPlatformID]} errMsg]
	close $fd
	
	if { $failed } {
		error $errMsg
	}
	return $names
}

 # _ReadFontFile $fd
 # -----------------
 # return a list of font-info  (usually just one font-info)
 # Each font-info is a dictionary
 # An error is raised if fontPath cannot be properly parsed.
proc _ReadFontFile { fd {fontPlatformID ""}} {
	set fontsInfo {}			
	set magicTag [read $fd 4]
	if { $magicTag == "ttcf" } {
		set fontsOffset [_ReadTTC_Header $fd] ;# one elem for each subfont
		foreach fontOffset $fontsOffset {
			 # go to the start of the subfont and skip the initial 'magicTag' 
			seek $fd [expr {$fontOffset+4}]
			lappend fontsInfo [_ReadSimpleFontFile $fd $fontPlatformID]
		}
	} elseif { $magicTag in  { "OTTO" "\x00\x01\x00\x00"  "typ1" "true" } } {
		lappend fontsInfo [_ReadSimpleFontFile $fd $fontPlatformID]		
	} else {
		error "Unrecognized magic-number for OpenType font: 0x[binary encode hex $magicTag]"
	}
	return $fontsInfo
}


 # _ReadTTCHeader $fd
 # ------------------			
 # scan the TTC header and 
 # returns a list of fontsOffset ( i.e. where each sub-font starts )
proc _ReadTTC_Header {fd} {
	binary scan [read $fd 8] SuSuIu  majorVersion minorVersion numFonts
	 #extract a list of 32bit integers 
	binary scan [read $fd [expr {4*$numFonts}]] "Iu*" fontsOffset 
	
	# NOTE: if majorVersion > 2 there can be a trailing digital-signature section
	#  ...  IGNORE IT	
	
	return $fontsOffset
}


 # _ReadSimpleFontFile $fd
 # -----------------------
 # returns a font-info dictionary (or an error ...)
proc _ReadSimpleFontFile {fd {fontPlatformID ""}} {
	 # Assert: we are at the beginng of the Table-Directory	
	binary scan [read $fd 8] SuSuSuSu  numTables searchRange entrySelector rangeShift

	 # scan the Table Directory ...	we are just interested with the 'name' table
	set tableName {}
	for {set n 0} {$n<$numTables} {incr n} {
		binary scan [read $fd 16] a4H8IuIu  tableName _checksum start length
		if { $tableName == "name" } break 
	}
	if { $tableName != "name" } {
		error "No \"name\" table found."
	}
	
	seek $fd $start		
	return [_ReadTable.name $fd $length $fontPlatformID]
}


 # _convertfromUTF16BE $data
 # -------------------------
 # convert strings from UTF16BE to (tcl)Unicode strings.
 # NOTE:
 # When font-info is extracted from namerecords with platformID==3 (Windows)
 # data (binary strings) are originally encoded in UTF16-BE.
 # These data should be converted in (tcl)Unicode strings.
 # Since the "tcl - unicode encoding" is BigEndian or LittleEndian, depending
 # on the current platform, two variants of _convertfromUTF16BE areprovided;
 # the right conversion will be choosen once at load-time.
if { $::tcl_platform(byteOrder) == "bigEndian" } {
	proc _convertfromUTF16BE {data} {
		encoding convertfrom unicode $data
	}
} else {
	proc _convertfromUTF16BE {data} {
		 # swp bytes, then call encoding unicode ..
		binary scan $data "S*" z 
		encoding convertfrom unicode [binary format "s*" $z]
	}
}

 # _score ...
 # --------------
 # return the score (>=0)
 # for every tuple of {platformID specificID languageID}
 # Scores are weighted based un the target fontPlatformID
proc _score {fontPlatformID platformID specificID languageID} {
	switch -- $fontPlatformID {
		1 { ;# macintosh i.e non-windows
			set premium(macEnglish) 0x0100
			set premium(winEnglish_US) 0x0040
			set premium(winEnglish_UK) 0x0030
			set premium(winEnglish) 0x0020
		}
		3 { ;# windows
			set premium(macEnglish) 0x000
			set premium(winEnglish_US) 0x0400
			set premium(winEnglish_UK) 0x0300
			set premium(winEnglish) 0x0200
		}
		default { error "unsupported target platformID" }
	}
	
	set score 0
	switch -- $platformID {
		0  { 
			# platform Unicode
			set score 3
		}
		1 {
			# platform Macintosh
			if { $specificID == 0 } {
				# MacEncodingRoman
				set score 2
			} else {
				return 0  ;# NO WAY !!
			}
			if { $languageID == 0 } {
				# MacLanguageEnglish
				incr score $premium(macEnglish)
			}
		}
		3 {
			#platform Windows
			switch -- $specificID {
				0 { set score 1 }  ;# WindowsEncodingSymbol
				1 { set score 4 }  ;# WindowsEncodingUCS2
				default { return 0 }  ;# NO WAY !!
			}
			  # try to augment score based on languageId 
			if { ($languageID & 0xFF) == 0x09 } {
				# .. generic English
				switch -- [format "0x%.4x" $languageID] {
					 0x0409 { incr score $premium(winEnglish_US) }
					 0x0809 { incr score $premium(winEnglish_UK) }
					default { incr score $premium(winEnglish) }
				}
			}
		}
	}
	return $score
}


 #  _ReadTable.name $fd
 # --------------------
 # Scan the 'name' table and return a font-info dictionary.
 #
 # Reference Specification:
 #    see http://www.microsoft.com/typography/otspec/name.htm
 # NOTE:
 # New internal logic for selecting values among repeated values
 #   for different platformID encodingID languageID nameID,
 #   based on a score system.
proc _ReadTable.name {fd tableSize {fontPlatformID ""}} {
	variable _NameID2Str
	
	if { $fontPlatformID eq "" } {
		if { $::tcl_platform(platform) == "windows" } {	
			set fontPlatformID 3
		} else {
			set fontPlatformID 1
		}
	}
	
	set tableStart [tell $fd]  ;# save the start of this Table
	set tableEnd [expr {$tableStart+$tableSize}]
	binary scan [read $fd 6] "SuSuSu"  version count strRegionOffset
	 # we expect version == 0 ; version == 1 is not supported yet
	
	set strRegionStart [expr {$tableStart + $strRegionOffset}]
	set strRegionSize  [expr {$tableSize-$strRegionOffset}]
	 #Each nameRecord is made of 6 UnsignedShort
	binary scan [read $fd [expr {2*6*$count}]] "Su*"  nameRecords
	
	set nameinfo [dict create]
	 # initialize bestScore array
	for {set nameID 0}  {$nameID <= 25} {incr nameID} {
		set bestScore($nameID) 0
		# no need to initialize bestPlatform, bestOffset, bestLength arrays
	}
	 # Assert: nameRecords are sorted by platformID,encodingID,languageID,nameID
	foreach { platformID specificID languageID nameID strLength strOffset } $nameRecords {
		if { $nameID > 25 } continue
		 # Offset could be anything if length is zero.
		if {$strLength == 0} continue
		# Fonts are full of wrong data, if the offset is outside of the string data we simply skip the record.
		if { $strOffset >= $strRegionSize || $strOffset+$strLength>$strRegionSize } continue ;#  WARNING
		
		set score [_score $fontPlatformID $platformID $specificID $languageID]
		if { $score > $bestScore($nameID) } {
			set bestScore($nameID) $score
			set bestOffset($nameID) $strOffset
			set bestLength($nameID) $strLength
			set bestPlatform($nameID) $platformID
		}
	}
	for {set nameID 0}  {$nameID <= 25} {incr nameID} {
		if { $bestScore($nameID) == 0 } continue;
		
		set offset $bestOffset($nameID)
		set length $bestLength($nameID)
		seek $fd [expr {$strRegionStart+$offset}]
		binary scan [read $fd $length] "a*" value
		
		 # Windows only: extracted strings from records with platformID == 3 (windows)
		 #  are in UTF-16BE format. They should be converted.
		if { $bestPlatform($nameID) == 3 } {
			set value [_convertfromUTF16BE $value]
		}
		
		set nameIDstr [dict get $_NameID2Str $nameID]
		dict set nameinfo $nameIDstr $value
	}
if 0 {
# TODO ...  The score logic should consider the current platform
# and then adjust the evaluation.
# BUT current platform should be an 'external' parameter, so that
# it could be used for tuning different platforms.

	 # prefer typographicFamily over fontFamily
	 if { [dict exists $nameinfo typographicFamily] } {
	 	dict set nameinfo fontFamily [dict get $nameinfo typographicFamily]
	 }
	 # prefer typographicSubfamily over fontSubfamily
	 if { [dict exists $nameinfo typographicSubfamily] } {
	 	dict set nameinfo fontSubfamily [dict get $nameinfo typographicSubfamily]
	 }
}	 
	  
	 # if $version == 1, there should be a 'languageTag section' 
	 #  ...  IGNORE IT 

	return $nameinfo
}
