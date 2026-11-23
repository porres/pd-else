
package ifneeded extrafont 1.0  [list apply { dir  {
	package require Tk
	
	set thisDir [file normalize ${dir}]

	set tail_libFile unknown
	set os $::tcl_platform(platform)
	switch -- $os {
		windows { set os win ; set tail_libFile extrafont.dll }
		unix    {
			set os $::tcl_platform(os)
			switch -- $os {
				Darwin { set os darwin ; set tail_libFile extrafont.dylib }
				Linux  { set os linux ;  set tail_libFile extrafont.so }
			}
		}
	}
	 # try to guess the tcl-interpreter architecture (32/64 bit) ...
	set arch $::tcl_platform(pointerSize)
	switch -- $arch {
		4 { set arch x32  }
		8 { set arch x64 }
		default { error "extrafont: Unsupported architecture: Unexpected pointer-size $arch!!! "}
	}
	
	
	set dir_libFile [file join $thisDir ${os}-${arch}]
	if { ! [file isdirectory $dir_libFile ] } {
		error "extrafont: Unsupported platform ${os}-${arch}"
	}

	set full_libFile [file join $dir_libFile $tail_libFile]			 
	load $full_libFile
	
	namespace eval extrafont {}
	source [file join $thisDir extrautils.tcl]
	
	package provide extrafont 1.0

}} $dir] ;# end of lambda apply


