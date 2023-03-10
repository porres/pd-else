# generate menu tree for objects from a textfile with nested Tcl lists (using {}).

package require pd_menus

namespace eval category_menu {
}

proc category_menu::load_menutree {} {
    set path [string trim $::sys_searchpath "\{ \}"]
    set file [list $path else else_tree.tcl]
    set f [open [file join {*}$file]]
    set menutree [read $f]
    close $f
    unset f        
    return $menutree
}

proc menu_send_else_obj {w x y item} {
    pdsend "$w obj $x $y else/$item"
}

proc category_menu::create {mymenu} {
    set menutree [load_menutree]
    $mymenu add separator
    foreach categorylist $menutree {
        set category [lindex $categorylist 0]
        menu $mymenu.$category
        $mymenu add cascade -label $category -menu $mymenu.$category
        foreach subcategorylist [lrange $categorylist 1 end] {
            set subcategory [lindex $subcategorylist 0]
            menu $mymenu.$category.$subcategory
            $mymenu.$category add cascade -label $subcategory -menu $mymenu.$category.$subcategory
            foreach item [lindex $subcategorylist end] {
                # replace the normal dash with a Unicode minus so that Tcl does not
                # interpret the dash in the -label to make it a separator
                $mymenu.$category.$subcategory add command \
                    -label [regsub -all {^\-$} $item {−}] \
                    -command "menu_send_else_obj \$::focused_window \$::popup_xcanvas \$::popup_ycanvas {$item}"
            }
        }
    }
}

category_menu::create .popup

::pdwindow::post "ELSE's object browser-plugin loaded via the 'else' binary\n"
