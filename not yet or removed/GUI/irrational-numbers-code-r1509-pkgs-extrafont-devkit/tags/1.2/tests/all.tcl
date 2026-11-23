# helper for running TTXN test-suite on "extrafont" package
# without "installing" them

# you can pass extra constraints 
#   -constraints userInteraction ?-constraints ...?

# Note that TTXN package should be placed under a standard tcl-library directory
# OR within this "test" directory

set thisDir [file dirname [file normalize [info script]]]

 # path for TTXN package
lappend auto_path $thisDir

 # path for extrafont package -- we assume it's under ../XBUILD
lappend auto_path [file join [file dirname $thisDir] XBUILD]

foreach testFile [glob $thisDir/*.test] {
	set interp [interp create] 
    $interp eval [list   set auto_path $auto_path]
    $interp eval [list   set tcl_interactive 1] ;# or tcltest will exit !!
	$interp eval [list set argv $argv]
    $interp eval [list   source $testFile]
    interp delete $interp
}
