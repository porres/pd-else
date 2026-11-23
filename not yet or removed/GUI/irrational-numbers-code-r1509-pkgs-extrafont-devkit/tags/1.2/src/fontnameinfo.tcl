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
	16 preferredFamily
	17 preferredSubfamily
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
    #  ? 16 preferredFamily
    #  ? 17 preferredSubFamily
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
 # An error is raised if fontPath cannot be properly parsed.
 proc nameinfo {fontPath} {
	set fd [open $fontPath "r"]
	fconfigure $fd -translation binary        
		set failed [catch {set names [_ReadFontFile $fd]} errMsg]	
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
proc _ReadFontFile { fd } {
	set fontsInfo {}			
	set magicTag [read $fd 4]
	if { $magicTag == "ttcf" } {
		set fontsOffset [_ReadTTC_Header $fd] ;# one elem for each subfont
		foreach fontOffset $fontsOffset {
			 # go to the start of the subfont and skip the initial 'magicTag' 
			seek $fd [expr {$fontOffset+4}]
			lappend fontsInfo [_ReadSimpleFontFile $fd]
		}
	} elseif { $magicTag in  { "OTTO" "\x00\x01\x00\x00"  "typ1" "true" } } {
		lappend fontsInfo [_ReadSimpleFontFile $fd]		
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
	
	# NOTE: if majorVersion > 2 there can be a trailing digital-signature section ....
	#  ...  IGNORE IT	
	
	return $fontsOffset
}


 # _ReadSimpleFontFile $fd
 # -----------------------
 # returns a font-info dictionary (or an error ...)
proc _ReadSimpleFontFile {fd} {
	 # Assert: we are at the beginng of the Table-Directory	
	binary scan [read $fd 8] SuSuSuSu  numTables searchRange entrySelector rangeShift       

	 # scan the Table Directory ...	we are just interested with the 'name' table
	set tableName {}
	for {set n 0} {$n<$numTables} {incr n} {
		binary scan [read $fd 16] a4H8IuIu  tableName _checksum start _length
		if { $tableName == "name" } break 
	}
	if { $tableName != "name" } {
		error "No \"name\" table found."
	}
	
	seek $fd $start		
	return [_ReadTable.name $fd]
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

 #  _ReadTable.name $fd
 # --------------------
 # Scan the 'name' table and return a font-info dictionary.
 #'  {
 # Reference Specification:
 #    see http://www.microsoft.com/typography/otspec/name.htm
 # NOTE:
 # We don't care to extract all the (repeated) info for different platforms,
 #  encodings, languages.
 # Running on Windows  we extract details from records having
 #    platform:  3 (windows)
 #    encoding:  1 (Unicode BMP (UCS-2))
 #    language: 0x0409  (English (US))
 #   (NOTE: The encoding UCS-2  is a subset of UTF-18BE)
 # Running on non-Windows, we extract details from records having
 #   platform: 1 (macintosh)
 #   encoding: 0 (macRoman)
 #   language: 0 (American English)

proc _ReadTable.name {fd} {
	variable _NameID2Str
	set startPos [tell $fd]  ;# save the start of this Table
	binary scan [read $fd 6] "SuSuSu"  format count stringOffset
	set storageStart [expr {$startPos + $stringOffset}]
	 #Each nameRecord is made of 6 UnsignedShort
	binary scan [read $fd [expr {2*6*$count}]] "Su*"  nameRecord      

	set nameinfo [dict create]
	 # Scan all the nameRecords.
	if { $::tcl_platform(platform) == "windows" } {
		set baseTriplet [binary format "SSS" 3 1 0x0409]	 
	} else {
		set baseTriplet [binary format "SSS" 1 0 0]	 
	}
	 # Assert: nameRecords are sorted by platformID,encodingID,languageID,nameID
	foreach { platformID encodingID languageID nameID length offset } $nameRecord {
		set currTriplet [binary format "SSS" $platformID $encodingID $languageID]
		set cmp [string compare -nocase $baseTriplet $currTriplet]
		if { $cmp == 0 } {
			seek $fd [expr {$storageStart+$offset}]
			binary scan [read $fd $length] "a*" value	
			if { $nameID > 25 } break ;# no reason to scan the rest of the table ..
			set nameIDstr [dict get $_NameID2Str $nameID]
			dict set nameinfo $nameIDstr $value    
		}
		if { $cmp == -1 } break ;# no reason the scan the rest of the table ..
	}
	 # Windows only: extracted strings from records with platformID == "windows"
	 #  ara in UTF-16BE format. They should be converted.
	if { $::tcl_platform(platform) == "windows" } {
		dict for {key data} $nameinfo {
			dict set nameinfo $key [_convertfromUTF16BE $data]
		}
	}
	 # if $format == 1, there should be a 'languageTag section' 
	 #  ...  IGNORE IT 

	return $nameinfo            
}
