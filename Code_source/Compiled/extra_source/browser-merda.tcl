# generate menu tree for native objects for the canvas right click popup
# code by Porres and Seb Shader

package require pd_menus

namespace eval category_merda_menu {
}

proc menu_send_merda_obj {w x y item} {
    pdsend "$w obj $x $y else/$item"
    pdsend "pd-$item.pd loadbang"
}

# set nested list
proc category_merda_menu::load_menutree {} {
    set menutree { 
        {merda
            {assorted
                {else}}
            {gui
                {knob numbox~ drum.seq bicoeff openfile oscope~}}
            {time
                {chrono datetime}}
            {fft
                {hann~ bin.shift~}}
        }
    }
    return $menutree
}

proc category_merda_menu::create {cmdstring code result op} {
    set mymenu [lindex $cmdstring 1]
    set x [lindex $cmdstring 3]
    set y [lindex $cmdstring 4]
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
                    -command "menu_send_merda_obj \$::focused_window $x $y {$item}"
            }
        }
    }
}

trace add execution ::pdtk_canvas::create_popup leave category_merda_menu::create
