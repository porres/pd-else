#include <m_pd.h>
#include <m_imp.h>
#include <g_canvas.h>

t_widgetbehavior dropzone_widgetbehavior;
static t_class *dropzone_class;

typedef struct _dropzone {
    t_object        x_obj;
    t_outlet       *x_outlet;
    t_symbol       *x_bindsym;
    t_canvas       *x_cv;
    t_glist        *x_glist;
    int             x_width, x_height;
    int             x_zoom;
    int             x_drag_over;
    char            x_tag_obj[32];
    char            x_tag_outline[32];
    char            x_tag_in[32];
    char            x_tag_out[32];
    char            x_tag_sel[32];
    char            x_tag_hover[32];
    char            x_tag_mb[64];
} t_dropzone;

static void dropzone_draw_hover(t_dropzone *x)
{
    int z = x->x_zoom;
    int x1 = text_xpix(&x->x_obj, x->x_glist), y1 = text_ypix(&x->x_obj, x->x_glist);
    int x2 = x1 + x->x_width*z, y2 = y1 + x->x_height*z;

    int xb = 2; // horizontal border
    int yb = 4; // vertical border

    if(x->x_drag_over) {
        int drag_box_width = 4 * x->x_zoom;
        int drag_box_margin = 2 * x->x_zoom;
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline grey -width %d -tags {%s %s}\n", x->x_cv, x1 + drag_box_margin, y1 + drag_box_margin, x2 - drag_box_margin, y2 - drag_box_margin, drag_box_width, x->x_tag_obj, x->x_tag_hover);
    }
    else {
        t_canvas *cv = glist_getcanvas(x->x_glist);
        pdgui_vmess(0, "crs", cv, "delete", x->x_tag_hover);
    }
}

static void dropzone_draw(t_dropzone *x, t_glist *glist){
    x->x_cv = glist_getcanvas(x->x_glist = glist);
    int z = x->x_zoom;
    int x1 = text_xpix(&x->x_obj, glist), y1 = text_ypix(&x->x_obj, glist);
    int x2 = x1 + x->x_width*z, y2 = y1 + x->x_height*z;

    int xb = 2; // horizontal border
    int yb = 4; // vertical border
    int w = x->x_width*z, h = x->x_height*z;

   dropzone_draw_hover(x);
   sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline black -width %d -tags {%s %s %s}\n", x->x_cv, x1, y1, x2, y2, x->x_zoom, x->x_tag_outline, x->x_tag_obj, x->x_tag_sel);
   sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags {%s %s}\n", x->x_cv, x1, y2-OHEIGHT*z, x1+IOWIDTH*z, y2, x->x_tag_in, x->x_tag_obj);
   sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags {%s %s}\n", x->x_cv, x1, y1, x1+IOWIDTH*z, y1 +OHEIGHT*z, x->x_tag_out, x->x_tag_obj);
}

static void dropzone_erase(t_dropzone *x, t_glist* glist){
    t_canvas *cv = glist_getcanvas(glist);
    pdgui_vmess(0, "crs", cv, "delete", x->x_tag_obj);
}

// ------------------------ widgetbehaviour-----------------------------
static void dropzone_getrect(t_gobj *z, t_glist *gl, int *xp1, int *yp1, int *xp2, int *yp2){
    t_dropzone* x = (t_dropzone *)z;
    *xp1 = text_xpix(&x->x_obj, gl);
    *yp1 = text_ypix(&x->x_obj, gl);
    *xp2 = text_xpix(&x->x_obj, gl) + x->x_width*x->x_zoom;
    *yp2 = text_ypix(&x->x_obj, gl) + x->x_height*x->x_zoom;
}

static void dropzone_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_dropzone *x = (t_dropzone *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    dx *= x->x_zoom, dy *= x->x_zoom;
    t_canvas *cv = glist_getcanvas(glist);
    pdgui_vmess(0, "crs ii", cv, "move", x->x_tag_obj, dx, dy);
    canvas_fixlinesfor(glist, (t_text*)x);
}

static void dropzone_select(t_gobj *z, t_glist *glist, int sel){
    t_dropzone *x = (t_dropzone *)z;
    t_canvas *cv = glist_getcanvas(glist);
    pdgui_vmess(0, "crs rs", cv, "itemconfigure",
        x->x_tag_sel, "-outline", sel ? "blue" : "black");
}

static void dropzone_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void dropzone_vis(t_gobj *z, t_glist *glist, int vis){
    t_dropzone* x = (t_dropzone*)z;
    x->x_cv = glist_getcanvas(x->x_glist = glist);
    vis ? dropzone_draw(x, glist) : dropzone_erase(x, glist);
    sys_vgui("::else_dnd::bind_dnd_canvas .x%lx\n", glist_getcanvas(glist));
}

static void dropzone_zoom(t_dropzone *x, t_floatarg f){
    x->x_zoom = (int)f;
}

static void dropzone_dim(t_dropzone *x, t_floatarg f1, t_floatarg f2){
    x->x_width = f1 > 5 ? f1 : 5;
    x->x_height = f2 > 5 ? f2 : 5;
    dropzone_vis((t_gobj*)x, x->x_glist, 0);
    dropzone_vis((t_gobj*)x, x->x_glist, 1);
}

void dropzone_drag_over(t_dropzone *x, t_symbol *s, int argc, t_atom *argv) {
    int mouse_x = (int)atom_getfloat(argv);
    int mouse_y = (int)atom_getfloat(argv + 1);

    int x1, y1, x2, y2;
    dropzone_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);

    x->x_drag_over = mouse_x >= x1 && mouse_x <= x2 && mouse_y >= y1 && mouse_y <= y2;
    dropzone_draw_hover(x);
}

static void dropzone_drag_leave(t_dropzone *x) {
    x->x_drag_over = 0;
    dropzone_draw_hover(x);
}

static void dropzone_drag_drop(t_dropzone *x,  t_symbol *s, int argc, t_atom *argv) {
    if (argc > 0 && argv[0].a_type == A_SYMBOL && x->x_drag_over) {
        outlet_symbol(x->x_outlet, atom_getsymbol(argv));
        x->x_drag_over = 0;
        dropzone_draw_hover(x);
    }
}

void *dropzone_new(t_symbol *s, int ac, t_atom *av) {
    t_dropzone *x = (t_dropzone *)pd_new(dropzone_class);
    x->x_outlet = outlet_new(&x->x_obj, &s_symbol);
    x->x_glist = (t_glist *)canvas_getcurrent();
    x->x_cv = canvas_getcurrent();
    x->x_zoom = x->x_cv->gl_zoom;
    x->x_width = ac >= 1 ? atom_getfloat(av) : 100;
    x->x_height = ac >= 2 ? atom_getfloat(av + 1) : 100;
    x->x_drag_over = 0;

    sprintf(x->x_tag_obj, "%pOBJ", x);
    sprintf(x->x_tag_in, "%pIN", x);
    sprintf(x->x_tag_out, "%pOUT", x);
    sprintf(x->x_tag_sel, "%pSEL", x);
    sprintf(x->x_tag_outline, "%pOUTLINE", x);
    sprintf(x->x_tag_hover, "%pHOVER", x);
    sprintf(x->x_tag_mb, ".x%lx.c.s%lx", (unsigned long)x->x_cv, (unsigned long)x);

    pd_bind(&x->x_obj.ob_pd, x->x_bindsym = gensym("__else_dnd_rcv"));
    return (void *)x;
}

void dropzone_free(t_dropzone *x) {
    pd_unbind(&x->x_obj.ob_pd, x->x_bindsym);
}

void dropzone_setup(void) {
    dropzone_class = class_new(gensym("dropzone"),
                               (t_newmethod)dropzone_new,
                               (t_method)dropzone_free,
                               sizeof(t_dropzone),
                               0, A_GIMME, 0);

    class_addmethod(dropzone_class, (t_method)dropzone_drag_over, gensym("_drag_over"), A_GIMME, 0);
    class_addmethod(dropzone_class, (t_method)dropzone_drag_leave, gensym("_drag_leave"), 0);
    class_addmethod(dropzone_class, (t_method)dropzone_drag_drop, gensym("_drag_drop"), A_GIMME, 0);
    class_addmethod(dropzone_class, (t_method)dropzone_zoom, gensym("zoom"), A_CANT, 0);
    class_addmethod(dropzone_class, (t_method)dropzone_dim, gensym("dim"), A_FLOAT, A_FLOAT, 0);

    dropzone_widgetbehavior.w_getrectfn  = dropzone_getrect;
    dropzone_widgetbehavior.w_displacefn = dropzone_displace;
    dropzone_widgetbehavior.w_selectfn   = dropzone_select;
    dropzone_widgetbehavior.w_deletefn   = dropzone_delete;
    dropzone_widgetbehavior.w_visfn      = dropzone_vis;
    dropzone_widgetbehavior.w_clickfn    = NULL;
    class_setwidget(dropzone_class, &dropzone_widgetbehavior);

    sys_vgui("set dir [file join %s tkdnd]\n"
    "package ifneeded tkdnd 2.9.5 \\\n"
    "  \"source \\{$dir/tkdnd.tcl\\} ; \\\n"
#if __APPLE__
    "   tkdnd::initialise \\{$dir\\} libtkdnd2.9.5.dylib tkdnd\"\n"
#elif defined(_WIN64)
    "   tkdnd::initialise \\{$dir\\} libtkdnd2.9.5.dll tkdnd\"\n"
#elif defined(__linux__)
    #if defined(__x86_64__) // Detect Linux x86_64 and ARM
    "   tkdnd::initialise \\{$dir\\} libtkdnd2.9.5-x64.so tkdnd\"\n"
    #elif defined(__aarch64__)
    "   tkdnd::initialise \\{$dir\\} libtkdnd2.9.5-arm.so tkdnd\"\n"
    #endif
#else
    #error "Unsupported operating system"
#endif
    "package require tkdnd\n"
    "namespace eval ::else_dnd {} \n"
    "proc ::else_dnd::correct_spaces {string_value} {\n"
    "    return [regsub -all {\\s+} $string_value \"\\\\ \"]\n"
    "}\n"
    "proc ::else_dnd::send_drag_coordinates {mytoplevel x y} {\n"
    "    set tkcanvas [tkcanvas_name $mytoplevel]\n"
    "    set scrollregion [$tkcanvas cget -scrollregion]\n"
    "    set left_xview_pix [expr {([lindex [$tkcanvas xview] 0] * ([lindex $scrollregion 2] - [lindex $scrollregion 0])) + [lindex $scrollregion 0]}]\n"
    "    set top_yview_pix [expr {([lindex [$tkcanvas yview] 0] * ([lindex $scrollregion 3] - [lindex $scrollregion 1])) + [lindex $scrollregion 1]}]\n"
    "    set xrel [expr int($x - [winfo rootx $mytoplevel] + $left_xview_pix)]\n"
    "    set yrel [expr int($y - [winfo rooty $mytoplevel] + $top_yview_pix)]\n"
    "    pdsend \"__else_dnd_rcv _drag_over $xrel $yrel \"\n"
    "}\n"
    "proc ::else_dnd::send_file_paths {files} {\n"
    "    foreach file $files {\n"
    "        pdsend \"__else_dnd_rcv _drag_drop [::else_dnd::correct_spaces [file normalize $file]] \"\n"
    "    }\n"
    "}\n"
    "proc ::else_dnd::bind_dnd_canvas {mytoplevel} {\n"
    "    ::tkdnd::drop_target register $mytoplevel DND_Files\n"
    "    bind $mytoplevel <<DropPosition>> { ::else_dnd::send_drag_coordinates %%W %%X %%Y}\n"
    "    bind $mytoplevel <<Drop:DND_Files>> { ::else_dnd::send_file_paths %%D }\n"
    "    bind $mytoplevel <<DropLeave>> { pdsend \"__else_dnd_rcv _drag_leave\"}\n"
    "}\n", dropzone_class->c_externdir->s_name);
}
