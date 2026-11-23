# futmp.tcl  - file utilities for working with temporary files.
#
# This package is largely derived from the tcllib's package "fileutil" 1.14.10
# The original commands' synospys has been changed to include an explicit
#  base-dir and an optional file-suffix.
#
#  futmp::tempdir
#  futmp::tempdir _newdir_
#  futmp::mktempdir _basedir_ ?_prefix_?
#  futmp::tempfile _basedir_ ?_prefix_? ?_suffix_?
#
# -Aug.2017 - A.Buratti fecit
#
# Credits for the original "fileutil" :
#  Copyright (c) 1998-2000 by Ajuba Solutions.
#  Copyright (c) 2005-2013 by Andreas Kupries <andreas_kupries@users.sourceforge.net>
#

package require Tcl 8.2
package provide futmp 1.14.10

namespace eval ::futmp {
	variable tempdir    {}
	variable tempdirSet 0
}

# ::futmp::tempdir --
#
#	Return the correct directory to use for temporary files.
#	Attempt this sequence, which seems logical:
#
#       1. The directory named by the `TMPDIR' environment variable.
#
#       2. The directory named by the `TEMP' environment variable.
#
#       3. The directory named by the `TMP' environment variable.
#
#       4. A platform-specific location:
#            * On Macintosh, the `Temporary Items' folder.
#
#            * On Windows, the directories `C:\\TEMP', `C:\\TMP',
#              `\\TEMP', and `\\TMP', in that order.
#
#            * On all other platforms, the directories `/tmp',
#              `/var/tmp', and `/usr/tmp', in that order.
#
#       5. As a last resort, the current working directory.
#
#	The code here also does
#
#	0. The directory set by invoking tempdir with an argument.
#	   If this is present it is used exclusively.
#
# Arguments:
#	None.
#
# Side Effects:
#	None.
#
# Results:
#	The directory for temporary files.

proc ::futmp::tempdir {args} {
	if {[llength $args] > 1} {
		return -code error {wrong#args: should be "::futmp::tempdir ?path?"}
	} elseif {[llength $args] == 1} {
		variable tempdir    [lindex $args 0]
		variable tempdirSet 1
		return
	}
	return [TempDir]
}


proc ::futmp::TempDir {} {
	global tcl_platform env
	variable tempdir
	variable tempdirSet
	
	set attempdirs [list]
	set problems   {}
	
	if {$tempdirSet} {
		lappend attempdirs $tempdir
		lappend problems {User/Application specified tempdir}
	} else {
		foreach tmp {TMPDIR TEMP TMP} {
			if { [info exists env($tmp)] } {
				lappend attempdirs $env($tmp)
			} else {
				lappend problems "No environment variable $tmp"
			}
		}
	
		switch $tcl_platform(platform) {
			windows {
				lappend attempdirs "C:\\TEMP" "C:\\TMP" "\\TEMP" "\\TMP"
			}
			macintosh {
				lappend attempdirs $env(TRASH_FOLDER)  ;# a better place?
			}
			default {
				lappend attempdirs \
					[file join / tmp] \
					[file join / var tmp] \
					[file join / usr tmp]
			}
		}
	
		lappend attempdirs [pwd]
	}

    foreach tmp $attempdirs {
		if { [file isdirectory $tmp] && [file writable $tmp] } {
		    return $tmp
		} elseif { ![file isdirectory $tmp] } {
	    	lappend problems "Not a directory: $tmp"
		} else {
	    	lappend problems "Not writable: $tmp"
		}
	}
	
	# Fail if nothing worked.
	return -code error "Unable to determine a proper directory for temporary files\n[join $problems \n]"
}


proc futmp::mktempdir { basedir {prefix {}} } {
	if {![file writable $basedir]} {
		return -code error "Base-Directory $basedir is not writable"
	}
	
	set chars       "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	set nrand_chars 10
	set maxtries    10
	
	for {set i 0} {$i < $maxtries} {incr i} {
		set newname $prefix
		for {set j 0} {$j < $nrand_chars} {incr j} {
			append newname [string index $chars [expr {int(rand()*62)}]]
		}
		set newname [file join $basedir $newname]
		
		if { ! [file exists $newname] } {
			# WARNING [file mkdir ..] does not return an error if $newname is already present
			# For this reason we test before if the new directory is present.
			# This is not perfect, since this should be an atomic operation. It isn't !
			# There's a chance that someone else will create the same $newname between
			#  these two operations. (Mitigation: the probability that another process
			#  generates another file with *same random name* in the *same interval (msec)*
			#  is very low. 
			if { ! [catch {file mkdir $newname}] } {
				return $newname
			}       
		}
	}
	return -code error "Failed to find an unused temporary file name"
}


# ::futmp::tempfile --
#
#   generate a temporary file name suitable for writing to
#   the file name will be unique, writable.
#   Code derived from http://mini.net/tcl/772 attributed to
#    Igor Volobouev and anon.
#
# Arguments:
#   basedir    - where to put the new filename
#   prefix     - prefix for the new filename, 
#   extension  - extension for the new filename
# Results:
#   returns an opened channed (and the filename via filenameVar) or an error.
#

proc ::futmp::tempfile { filenameVar basedir {prefix {}} {extension {}} } {
	set chars       "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	set nrand_chars 10
	set maxtries    10
	set access      [list RDWR CREAT EXCL]
	set permission  0600
	set channel ""

	if {![file writable $basedir]} {
		return -code error "Directory $basedir is not writable"
	}

    for {set i 0} {$i < $maxtries} {incr i} {
	 	set newname $prefix
	 	for {set j 0} {$j < $nrand_chars} {incr j} {
	 	    append newname [string index $chars [expr {int(rand()*62)}]]
	 	}
		set newname [file join $basedir $newname]
	    append newname $extension
		if { ! [catch {open $newname $access $permission} channel] } {
			# Success
			upvar $filenameVar newfilename
			set newfilename $newname
			return $channel
		}
    }
 	return -code error "Failed to find an unused temporary file name"
}
