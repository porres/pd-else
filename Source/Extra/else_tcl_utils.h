// Shared Tcl utilities.

#ifndef ELSE_TCL_UTILS_H
#define ELSE_TCL_UTILS_H

static const char *else_tcl_utils =
// ------------------------------------------------------------------
// color2hex: convert any Tk-valid color spec (#rgb, #rrggbb, named)
// to a canonical #rrggbb string. Returns "" on failure.
// ------------------------------------------------------------------
"if {![llength [info commands color2hex]]} {\n"
"proc color2hex {name} {\n"
"    if {[catch {winfo rgb . $name} rgb]} {\n"
"        return \"\"\n"
"    }\n"
"    set r [format %02x [expr {[lindex $rgb 0] / 256}]]\n"
"    set g [format %02x [expr {[lindex $rgb 1] / 256}]]\n"
"    set b [format %02x [expr {[lindex $rgb 2] / 256}]]\n"
"    return \"#$r$g$b\"\n"
"}\n"
"}\n"
// ------------------------------------------------------------------
// get_symbol: unescape a Pd symbol string (e.g. from dialog input).
// ------------------------------------------------------------------
"if {![llength [info commands get_symbol]]} {\n"
"proc get_symbol {text} {\n"
"    set sym [subst -nocommands -novariables $text]\n"
"    if {$sym eq \"empty\"} {set sym \"\"}\n"
"    return $sym\n"
"}\n"
"}\n";

#endif
