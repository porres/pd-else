#include "m_pd.h"

typedef struct _lock{
    t_object   x_ob;
}t_lock;

static t_class *lock_class;

static void lock_float(t_floatarg f){
    if(f > 0){
        sys_vgui("proc ::pdtk_canvas::pdtk_canvas_popup {mytoplevel xcanvas ycanvas hasproperties hasopen} {\n");
        sys_vgui("}\n");
    }
    else{
        sys_vgui("proc ::pdtk_canvas::pdtk_canvas_popup {mytoplevel xcanvas ycanvas hasproperties hasopen} {\n");
        sys_vgui("  set ::popup_xcanvas $xcanvas\n");
        sys_vgui("  set ::popup_ycanvas $ycanvas\n");
        sys_vgui("  if {$hasproperties} {\n");
        sys_vgui("      .popup entryconfigure [_ \"Properties\"] -state normal\n");
        sys_vgui("  } else {\n");
        sys_vgui("      .popup entryconfigure [_ \"Properties\"] -state disabled\n");
        sys_vgui("  } \n");
        sys_vgui("  if {$hasopen} {\n");
        sys_vgui("      .popup entryconfigure [_ \"Open\"] -state normal\n");
        sys_vgui("  } else {\n");
        sys_vgui("      .popup entryconfigure [_ \"Open\"] -state disabled\n");
        sys_vgui("  } \n");
        sys_vgui("  set tkcanvas [tkcanvas_name $mytoplevel] \n");
        sys_vgui("  set scrollregion [$tkcanvas cget -scrollregion] \n");
        sys_vgui("  set left_xview_pix [expr [lindex [$tkcanvas xview] 0] * [lindex $scrollregion 2]] \n");
        sys_vgui("  set top_yview_pix [expr [lindex [$tkcanvas yview] 0] * [lindex $scrollregion 3]] \n");
        sys_vgui("  set xpopup [expr int($xcanvas + [winfo rootx $tkcanvas] - $left_xview_pix)] \n");
        sys_vgui("  set ypopup [expr int($ycanvas + [winfo rooty $tkcanvas] - $top_yview_pix)] \n");
        sys_vgui("  tk_popup .popup $xpopup $ypopup 0 \n");
        sys_vgui("}\n");
    }
}

static void *lock_new(void){
    t_lock *x = (t_lock *)pd_new(lock_class);
    return (x);
}

void lock_setup(void){
    lock_class = class_new(gensym("lock"), (t_newmethod)lock_new,
        0, sizeof(t_lock), 0, 0);
    class_addfloat(lock_class, (t_method)lock_float);
}
