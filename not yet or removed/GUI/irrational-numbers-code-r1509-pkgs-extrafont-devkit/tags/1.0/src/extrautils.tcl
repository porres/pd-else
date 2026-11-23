# Just some sugar code

# to be included in extrafont namespace

proc extrafont::isAvailable {family} {
	expr [lsearch -nocase -exact [font families] $family] == -1 ? false : true
}

proc extrafont::availableFamilies { {familyPattern {*}} } {
	set L {}
	foreach family [font families] {
		if { [string match -nocase $familyPattern $family] } {
			lappend L $family
		}
	}
	return $L
}
