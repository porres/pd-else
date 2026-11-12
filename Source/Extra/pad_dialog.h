// pad's properties dialog

static const char *pad_dialog_tcl =
"namespace eval ::dialog_pad:: {\n"
"}\n"
// arrays to store per-dialog values
"array set ::dialog_pad::var_width {} ;\n"
"array set ::dialog_pad::var_height {} ;\n"
"\n"
"array set ::dialog_pad::var_color_bg {} ;\n"
"array set ::dialog_pad::var_pd_bg {} ;\n"
"array set ::dialog_pad::var_hex_bgcolor_text {} ;\n"
"\n"
"array set ::dialog_pad::var_theme {} ;\n"
"array set ::dialog_pad::var_transp {} ;\n"
"\n"

// Get parameters from Pd when asking for properties!
"proc pad_dialog {id \\\n"
"         width height \\\n"
"         bcol pd_bg \\\n"
"         theme transp} {\n"
// The vid indicates the instance ID of this dialog
"    set vid [string trimleft $id .]\n"
// initialize the array with received values for this dialog instance
"    set ::dialog_pad::var_width($vid) $width\n"
"    set ::dialog_pad::var_height($vid) $height\n"
// var_color_* ALWAYS holds the custom color (what apply sends to C side)
"    set ::dialog_pad::var_color_bg($vid) $bcol\n"
"    set ::dialog_pad::var_pd_bg($vid) $pd_bg\n"
"    set ::dialog_pad::var_theme($vid) $theme\n"
"    set ::dialog_pad::var_transp($vid) $transp\n"
// Display: theme on shows Pd background color, theme off shows custom color.
// var_color_bg is NOT touched.
"    if { $theme == 1 } {\n"
"        set ::dialog_pad::var_hex_bgcolor_text($vid) [string toupper $pd_bg]\n"
"        set swatch_bg $pd_bg\n"
"    } else {\n"
"        set ::dialog_pad::var_hex_bgcolor_text($vid) [string toupper $bcol]\n"
"        set swatch_bg $bcol\n"
"    }\n"
"\n"

// HELPER PROCEDURES/FUNCTIONS

"proc ::dialog_pad::applymacos {id} {\n"
"    if {$::windowingsystem eq \"aqua\"} {\n"
"        ::dialog_pad::apply $id\n"
"    }\n"
"}\n"
"\n"

// theme_check: only update DISPLAY (hex entry + swatch) and control states.
// var_color_bg ALWAYS keeps the custom color, so toggling theme on/off
// never corrupts it.
"proc ::dialog_pad::theme_check {id} {\n"
"    set vid [string trimleft $id .]\n"
"    if { $::dialog_pad::var_theme($vid) == 1 } {\n"
//      Show Pd theme color in display only
"        set ::dialog_pad::var_hex_bgcolor_text($vid) [string toupper $::dialog_pad::var_pd_bg($vid)]\n"
"        $id.colors.bgrow.swatch configure -background $::dialog_pad::var_pd_bg($vid)\n"
"        $id.colors.bgrow.ent configure -state disabled\n"
"        $id.colors.radio.but configure -state disabled\n"
"    } else {\n"
//      Show custom color (still stored unchanged in var_color_bg)
"        set ::dialog_pad::var_hex_bgcolor_text($vid) [string toupper $::dialog_pad::var_color_bg($vid)]\n"
"        $id.colors.bgrow.swatch configure -background $::dialog_pad::var_color_bg($vid)\n"
"        $id.colors.radio.but configure -state normal\n"
"        if { $::dialog_pad::var_transp($vid) == 0 } {\n"
"            $id.colors.bgrow.ent configure -state normal\n"
"        }\n"
"    }\n"
"    ::dialog_pad::applymacos $id\n"
"}\n"

"proc ::dialog_pad::transp_check {id} {\n"
"    set vid [string trimleft $id .]\n"
"    if { $::dialog_pad::var_transp($vid) == 1 } {\n"
"        $id.colors.bgrow.ent configure -state disabled\n"
"    } else {\n"
"        if { $::dialog_pad::var_theme($vid) == 0 } {\n"
"            $id.colors.bgrow.ent configure -state normal\n"
"        }\n"
"    }\n"
"    ::dialog_pad::applymacos $id\n"
"}\n"

// preset_col: set the background color from preset hex (writes custom color + display)
"proc ::dialog_pad::preset_col {id presetcol} {\n"
"    set vid [string trimleft $id .]\n"
"    if { $::dialog_pad::var_theme($vid) == 1 } {\n"
"        return\n"
"    }\n"
"    set uppercol [string toupper $presetcol]\n"
"    set ::dialog_pad::var_color_bg($vid) $presetcol\n"
"    set ::dialog_pad::var_hex_bgcolor_text($vid) $uppercol\n"
"    $id.colors.bgrow.swatch configure -background $presetcol\n"
"    ::dialog_pad::applymacos $id\n"
"}\n"

// compose_color: open color picker for background
"proc ::dialog_pad::compose_color {id} {\n"
"    set vid [string trimleft $id .]\n"
"    set title [_ \"Background color\"]\n"
"    set color $::dialog_pad::var_color_bg($vid)\n"
"    set color [tk_chooseColor -title $title -initialcolor $color]\n"
"    if { $color ne \"\" } {\n"
"        ::dialog_pad::preset_col $id $color\n"
"    }\n"
"}\n"

// update_swatch: trace callback on hex entry changes
"proc ::dialog_pad::update_swatch {id varName index op} {\n"
"    set vid [string trimleft $id .]\n"
"    upvar #0 $varName var\n"
"    set hexval $var($vid)\n"
"    if {[regexp {^#[0-9A-Fa-f]{6}$} $hexval]} {\n"
"        $id.colors.bgrow.swatch configure -background $hexval\n"
"    }\n"
"}\n"

// update_color_from_hex_enter: validate hex entry (macOS enter-to-apply)
"proc ::dialog_pad::update_color_from_hex_enter {id} {\n"
"    set vid [string trimleft $id .]\n"
"    set hexval $::dialog_pad::var_hex_bgcolor_text($vid)\n"
"    if {[regexp {^#[0-9A-Fa-f]{6}$} $hexval]} {\n"
"        set ::dialog_pad::var_color_bg($vid) $hexval\n"
"    }\n"
"    if {$::windowingsystem eq \"aqua\"} {\n"
"        ::dialog_pad::bind_enter_to_apply $id\n"
"    }\n"
"}\n"

// DRAW PROPERTIES' WINDOW

"    toplevel $id -class DialogWindow\n"
"    wm title $id {[pad] Properties}\n"
"    wm group $id .\n"
"    wm resizable $id 0 0\n"
"    wm transient $id $::focused_window\n"
"    $id configure -menu $::dialog_menubar\n"
"    $id configure -padx 0 -pady 0\n"
"    ::pd_bindings::dialog_bindings $id \"pad\"\n"
"\n"

// 'dim' (Width and Height)
"    labelframe $id.dim -borderwidth 1 -pady 8 -padx 60 -text [_ \"Dimensions:\"]\n"
"    pack $id.dim -side top -fill x -pady 5\n"
"    frame $id.dim.fr\n"
"    label $id.dim.fr.w_lab -text [_ \"Width: \"]\n"
"    entry $id.dim.fr.w_ent -textvariable ::dialog_pad::var_width($vid) -width 5\n"
"    label $id.dim.fr.dummy -text \"\" -width 1\n"
"    label $id.dim.fr.h_lab -text [_ \"Height: \"]\n"
"    entry $id.dim.fr.h_ent -textvariable ::dialog_pad::var_height($vid) -width 5\n"
"    pack $id.dim.fr -side left -expand 1\n"
"    pack $id.dim.fr.w_lab $id.dim.fr.w_ent $id.dim.fr.dummy $id.dim.fr.h_lab $id.dim.fr.h_ent -side left\n"
"\n"

// Colors frame (single bg row + theme/transparent + compose + presets)
"    labelframe $id.colors -borderwidth 1 -text [_ \"Color:\"] -padx 5 -pady 8\n"
"    pack $id.colors -fill x\n"
// Background row (initial swatch color from $swatch_bg, chosen by theme above)
"    frame $id.colors.bgrow\n"
"    pack $id.colors.bgrow -side top -fill x -pady 2\n"
"    label $id.colors.bgrow.lab -text [_ \"Background:\"] -width 12 -anchor e\n"
"    entry $id.colors.bgrow.ent -textvariable ::dialog_pad::var_hex_bgcolor_text($vid) -width 10\n"
"    label $id.colors.bgrow.swatch -background $swatch_bg -relief ridge -width 24 -height 1\n"
"    pack $id.colors.bgrow.lab $id.colors.bgrow.ent $id.colors.bgrow.swatch -side left -padx 2\n"
"    trace add variable ::dialog_pad::var_hex_bgcolor_text($vid) write \\\n"
"        [list ::dialog_pad::update_swatch $id]\n"
// Disable if theme on / transparent on
"    if { $::dialog_pad::var_theme($vid) == 1 } {\n"
"       $id.colors.bgrow.ent configure -state disabled\n"
"    }\n"
"    if { $::dialog_pad::var_transp($vid) == 1 } {\n"
"       $id.colors.bgrow.ent configure -state disabled\n"
"    }\n"
"\n"
// Checkboxes: theme + transparent
"    frame $id.colors.checkboxes\n"
"    pack $id.colors.checkboxes -side top -fill x\n"
"    frame $id.colors.checkboxes.theme\n"
"    label $id.colors.checkboxes.theme.lab -text [_ \"Use Theme: \"]\n"
"    checkbutton $id.colors.checkboxes.theme.ent -variable ::dialog_pad::var_theme($vid) -width 5\\\n"
"       -command \"::dialog_pad::theme_check $id\"\n"
"    pack $id.colors.checkboxes.theme.ent $id.colors.checkboxes.theme.lab -side right -anchor e\n"
"    frame $id.colors.checkboxes.transp\n"
"    label $id.colors.checkboxes.transp.lab -text [_ \"Transparent Background: \"]\n"
"    checkbutton $id.colors.checkboxes.transp.ent -variable ::dialog_pad::var_transp($vid) -width 5\\\n"
"       -command \"::dialog_pad::transp_check $id\"\n"
"    pack $id.colors.checkboxes.transp.ent $id.colors.checkboxes.transp.lab -side right -anchor e\n"
"    pack $id.colors.checkboxes.theme $id.colors.checkboxes.transp -side left -anchor center\n"
"    $id.colors.checkboxes config -padx 80\n"
"\n"
// Compose button
"    frame $id.colors.radio\n"
"    pack $id.colors.radio -side top\n"
"    button $id.colors.radio.but -text [_ \"Compose\"] -command \"::dialog_pad::compose_color $id\"\n"
"    pack $id.colors.radio.but -side left\n"
"    if { $::dialog_pad::var_theme($vid) == 1 } {\n"
"       $id.colors.radio.but configure -state disabled\n"
"    }\n"
"\n"
// Preset colors, color scheme by Mary Ann Benedetto http://piR2.org
"    frame $id.colors.presets -pady 8\n"
"    pack $id.colors.presets -fill x\n"
"    foreach r {r1 r2 r3} hexcols {\n"
"       { \"#FFFFFF\" \"#DFDFDF\" \"#BBBBBB\" \"#FFC7C6\" \"#FFE3C6\" \"#FEFFC6\" \"#C6FFC7\" \"#C6FEFF\" \"#C7C6FF\" \"#E3C6FF\" }\n"
"       { \"#9F9F9F\" \"#7C7C7C\" \"#606060\" \"#FF0400\" \"#FF8300\" \"#FAFF00\" \"#00FF04\" \"#00FAFF\" \"#0400FF\" \"#9C00FF\" }\n"
"       { \"#404040\" \"#202020\" \"#000000\" \"#551312\" \"#553512\" \"#535512\" \"#0F4710\" \"#0E4345\" \"#131255\" \"#2F004D\" } }\\\n"
"    {\n"
"       frame $id.colors.presets.$r\n"
"       pack $id.colors.presets.$r -side top\n"
"       foreach i { 0 1 2 3 4 5 6 7 8 9} hexcol $hexcols \\\n"
"           {\n"
"               label $id.colors.presets.$r.c$i -background $hexcol -activebackground $hexcol -relief ridge -padx 7 -pady 0 -width 1\n"
"               bind $id.colors.presets.$r.c$i <Button> \"::dialog_pad::preset_col $id $hexcol\"\n"
"               if { $::dialog_pad::var_theme($vid) == 1 } {\n"
"                   $id.colors.presets.$r.c$i configure -state disabled\n"
"               }\n"
"           }\n"
"       pack $id.colors.presets.$r.c0 $id.colors.presets.$r.c1 $id.colors.presets.$r.c2 $id.colors.presets.$r.c3 \\\n"
"           $id.colors.presets.$r.c4 $id.colors.presets.$r.c5 $id.colors.presets.$r.c6 $id.colors.presets.$r.c7 \\\n"
"           $id.colors.presets.$r.c8 $id.colors.presets.$r.c9 -side left\n"
"    }\n"

// Cancel, Apply and OK buttons
"    frame $id.cao -pady 4\n"
"    pack $id.cao -side top\n"
"    button $id.cao.cancel -text [_ \"Cancel\"] \\\n"
"        -command \"::dialog_pad::cancel $id\"\n"
"    pack $id.cao.cancel -side left -expand 1 -fill x -padx 15 -ipadx 10\n"
"    if {$::windowingsystem ne \"aqua\"} {\n" // INCLUDE APPLY BUTTON ONLY IF NOT IN MACOS!
"        button $id.cao.apply -text [_ \"Apply\"] \\\n"
"            -command \"::dialog_pad::apply $id\"\n"
"        pack $id.cao.apply -side left -expand 1 -fill x -padx 15 -ipadx 10\n"
"    }\n"
"    button $id.cao.ok -text [_ \"OK\"] \\\n"
"        -command \"::dialog_pad::ok $id\" -default active\n"
"    pack $id.cao.ok -side left -expand 1 -fill x -padx 15 -ipadx 10\n"

// -----------------------------------------------------------------------------------
// BUTTON FUNCTIONS

//  CANCEL button
"proc ::dialog_pad::cancel {id} {\n"
"    pdsend \"$id cancel\"\n"
"}\n"
"\n"

// OK button
"proc ::dialog_pad::ok {id} {\n"
"    ::dialog_pad::apply $id\n"
"    ::dialog_pad::cancel $id\n"
"}\n"

// APPLY BUTTON: Send current values to Pd
"proc ::dialog_pad::apply {id} {\n"
"    set vid [string trimleft $id .]\n"
"    pdsend [concat $id dialog \\\n"
"                $::dialog_pad::var_width($vid) \\\n"
"                $::dialog_pad::var_height($vid) \\\n"
"                [string tolower $::dialog_pad::var_color_bg($vid)] \\\n"
"                $::dialog_pad::var_theme($vid) \\\n"
"                $::dialog_pad::var_transp($vid) \\\n"
"            ]\n"
"}\n"
// Bind and unbind enter key to Apply button on macOS for entry widgets
"    if {$::windowingsystem eq \"aqua\"} {\n"
// call apply on Return in entry boxes that are in focus & rebind Return to ok button
"        bind $id.dim.fr.w_ent <KeyPress-Return> \"::dialog_pad::bind_enter_to_apply $id\"\n"
"        bind $id.dim.fr.h_ent <KeyPress-Return> \"::dialog_pad::bind_enter_to_apply $id\"\n"
"        bind $id.colors.bgrow.ent <KeyPress-Return> \"::dialog_pad::update_color_from_hex_enter $id\"\n"
// unbind Return from ok button when an entry takes focus
"        $id.dim.fr.w_ent config -validate focusin -vcmd \"::dialog_pad::unbind_return $id\"\n"
"        $id.dim.fr.h_ent config -validate focusin -vcmd \"::dialog_pad::unbind_return $id\"\n"
"        $id.colors.bgrow.ent config -validate focusin -vcmd \"::dialog_pad::unbind_return $id\"\n"
// remove non-activatable widgets from focus list
"        $id.cao.cancel config -takefocus 0\n"
"        $id.colors.checkboxes.theme.ent config -takefocus 0\n"
"        $id.colors.checkboxes.transp.ent config -takefocus 0\n"
"        $id.colors.radio.but config -takefocus 0\n"
// show active focus on the ok button as it *is* activated on Return
"        $id.cao.ok config -default normal\n"
"        bind $id.cao.ok <FocusIn> \"$id.cao.ok config -default active\"\n"
"        bind $id.cao.ok <FocusOut> \"$id.cao.ok config -default normal\"\n"
// since we show the active focus, disable the highlight outline
"        $id.cao.ok config -highlightthickness 0\n"
"        $id.cao.cancel config -highlightthickness 0\n"
"    }\n"
"    $id.dim.fr.w_ent select from 0\n"
"    $id.dim.fr.w_ent select adjust end\n"
"    focus $id.dim.fr.w_ent\n"
"    position_over_window $id $::focused_window\n"
"}\n"
"\n"
// bind proc
"proc ::dialog_pad::bind_enter_to_apply {id} {\n"
"    ::dialog_pad::apply $id\n"
"    bind $id <KeyPress-Return> \"::dialog_pad::ok $id\"\n"
"    focus $id.cao.ok\n"
"    return 0\n"
"}\n"
"\n"
// unbind proc
"proc ::dialog_pad::unbind_return {id} {\n"
"    bind $id <KeyPress-Return> break\n"
"    return 1\n"
"}\n";
