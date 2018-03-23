
proc link_open {filename dir} {
    if {[string first "://" $filename] > -1} {
        menu_openfile $filename
    } elseif {[file pathtype $filename] eq "absolute"} {
        menu_openfile $filename
    } elseif {[file exists [file join $dir $filename]]} {
        set fullpath [file normalize [file join $dir $filename]]
        set dir [file dirname $fullpath]
        set filename [file tail $fullpath]
        menu_doc_open $dir $filename
    } else {
        pdtk_post "\[link\] file not found: $filename\n"
    }
}
