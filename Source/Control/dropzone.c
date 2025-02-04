#include <m_pd.h>
#include <m_imp.h>
#include <g_canvas.h>

t_widgetbehavior dropzone_widgetbehavior;
static t_class *dropzone_class, *dnd_proxy_class;

typedef struct _dnd_proxy {
    t_object               p_obj;
    struct _dropzone      *p_dropzone;
    t_clock               *p_clock;
    t_symbol              *p_sym;
    t_symbol              *p_cnv_sym;
    t_symbol              *p_parent_sym;
} t_dnd_proxy;

typedef struct _dropzone {
    t_object        x_obj;
    t_outlet       *x_outlet;
    t_canvas       *x_cv;
    t_glist        *x_glist;
    t_dnd_proxy    *x_proxy;
    int             x_width, x_height;
    int             x_zoom;
    int             x_drag_over;
    int             x_active;
    int             x_hover;
    int             x_edit;
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

    if(x->x_drag_over && x->x_hover) {
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
    int xb = 2;
    int yb = 4;
    int w = x->x_width*z, h = x->x_height*z;

   dropzone_draw_hover(x);
   sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline black -width %d -tags {%s %s %s}\n", x->x_cv, x1, y1, x2, y2, x->x_zoom, x->x_tag_outline, x->x_tag_obj, x->x_tag_sel);

   if(x->x_edit) {
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags {%s %s}\n", x->x_cv, x1, y2-OHEIGHT*z, x1+IOWIDTH*z, y2, x->x_tag_in, x->x_tag_obj);
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags {%s %s}\n", x->x_cv, x1, y1, x1+IOWIDTH*z, y1 +OHEIGHT*z, x->x_tag_out, x->x_tag_obj);
   }
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
    if(vis) {
        char buf[MAXPDSTRING];
        snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)x->x_cv);
        x->x_proxy->p_parent_sym = gensym(buf);

        sys_vgui("::else_dnd::bind_dnd_canvas .x%lx\n", x->x_cv);
        dropzone_draw(x, glist);
    }
    else {
        dropzone_erase(x, glist);
    }
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

static void dropzone_width(t_dropzone *x, t_floatarg f1){
    x->x_width = f1 > 5 ? f1 : 5;
    dropzone_vis((t_gobj*)x, x->x_glist, 0);
    dropzone_vis((t_gobj*)x, x->x_glist, 1);
}

static void dropzone_height(t_dropzone *x, t_floatarg f1){
    x->x_height = f1 > 5 ? f1 : 5;
    dropzone_vis((t_gobj*)x, x->x_glist, 0);
    dropzone_vis((t_gobj*)x, x->x_glist, 1);
}

static void dropzone_active(t_dropzone *x, t_floatarg f1){
    x->x_active = f1;
}

static void dropzone_hover(t_dropzone *x, t_floatarg f1){
    x->x_hover = f1;
}

static void dnd_proxy_free(t_dnd_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    pd_unbind(&p->p_obj.ob_pd, p->p_cnv_sym);
}

static t_dnd_proxy * dnd_proxy_new(t_dropzone *x, t_symbol *s){
    t_dnd_proxy *p = (t_dnd_proxy*)pd_new(dnd_proxy_class);
    // Bind for dnd messages
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    // Bind for canvas messages (editmode)
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)x->x_cv);
    p->p_cnv_sym = gensym(buf);
    pd_bind(&p->p_obj.ob_pd, p->p_cnv_sym);
    p->p_dropzone = x;
    p->p_clock = clock_new(p, (t_method)dnd_proxy_free);
    return(p);
}

static void dnd_proxy_any(t_dnd_proxy *p, t_symbol *s, int ac, t_atom *av){
    int edit = ac = 0;
    if(p->p_dropzone){
        if(s == gensym("editmode"))
            edit = (int)(av->a_w.w_float);
        else if(s == gensym("obj") || s == gensym("msg") || s == gensym("floatatom")
        || s == gensym("symbolatom") || s == gensym("text") || s == gensym("bng")
        || s == gensym("toggle") || s == gensym("numbox") || s == gensym("vslider")
        || s == gensym("hslider") || s == gensym("vradio") || s == gensym("hradio")
        || s == gensym("vumeter") || s == gensym("mycnv") || s == gensym("selectall")){
            edit = 1;
        }
        else
            return;
        if(p->p_dropzone->x_edit != edit){
            p->p_dropzone->x_edit = edit;

            t_canvas *cv = glist_getcanvas(p->p_dropzone->x_glist);
            dropzone_erase(p->p_dropzone, cv);
            dropzone_draw(p->p_dropzone, cv);
        }
    }
}

static void dnd_proxy_drag_over(t_dnd_proxy *p, t_symbol *s, int argc, t_atom *argv) {
    t_dropzone* dropzone = p->p_dropzone;
    t_symbol* bindsym = atom_getsymbol(argv);
    if(bindsym != p->p_parent_sym) return;

    if(dropzone) {
        int mouse_x = (int)atom_getfloat(argv + 1);
        int mouse_y = (int)atom_getfloat(argv + 2);

        int x1, y1, x2, y2;
        dropzone_getrect((t_gobj *)dropzone, dropzone->x_glist, &x1, &y1, &x2, &y2);

        int was_over = dropzone->x_drag_over;
        dropzone->x_drag_over = dropzone->x_active && mouse_x >= x1 && mouse_x <= x2 && mouse_y >= y1 && mouse_y <= y2;

        if(dropzone->x_drag_over) {
            t_atom args[2];
            SETFLOAT(args, mouse_x - x1);
            SETFLOAT(args + 1, mouse_y - y1);
            outlet_anything(dropzone->x_outlet, gensym("pos"), 2, args);
            if(!was_over)
            {
                SETFLOAT(args, 1.0);
                outlet_anything(dropzone->x_outlet, gensym("over"), 1, args);
            }
        }
        else {
            if(was_over)
            {
                t_atom arg;
                SETFLOAT(&arg, 0.0);
                outlet_anything(dropzone->x_outlet, gensym("over"), 1, &arg);
            }
        }

        dropzone_draw_hover(dropzone);
    }
}

static void dnd_proxy_drag_leave(t_dnd_proxy *p) {
    t_dropzone* dropzone = p->p_dropzone;
    if(dropzone) {
        if(dropzone->x_drag_over)
        {
            t_atom arg;
            SETFLOAT(&arg, 0.0);
            outlet_anything(dropzone->x_outlet, gensym("over"), 1, &arg);
        }
        dropzone->x_drag_over = 0;
        dropzone_draw_hover(dropzone);
    }
}

static void dnd_proxy_drag_drop(t_dnd_proxy *p,  t_symbol *s, int argc, t_atom *argv) {
     t_dropzone* dropzone = p->p_dropzone;
     t_symbol* bindsym = atom_getsymbol(argv);
    if (dropzone && bindsym == p->p_parent_sym && dropzone->x_drag_over) {
        if(atom_getfloat(argv + 1) == 0) {
            outlet_anything(dropzone->x_outlet, gensym("file"), argc - 2, argv + 2);
        }
        else {
            outlet_anything(dropzone->x_outlet, gensym("text"), argc - 2, argv + 2);
        }

        dropzone->x_drag_over = 0;

        t_atom arg;
        SETFLOAT(&arg, 0.0);
        outlet_anything(dropzone->x_outlet, gensym("over"), 1, &arg);

        dropzone_draw_hover(dropzone);
    }
}

static void *dropzone_new(t_symbol *s, int ac, t_atom *av) {
    int w = 127, h = 127;
    while(ac > 0) {
        if(av->a_type == A_SYMBOL){
            s = atom_getsymbolarg(0, ac, av);
            if(s == gensym("-dim")){
                if(ac >= 3 && (av+1)->a_type == A_FLOAT && (av+2)->a_type == A_FLOAT){
                    w = atom_getfloatarg(1, ac, av);
                    h = atom_getfloatarg(2, ac, av);
                    ac-=3, av+=3;
                }
                else goto errstate;
            }
            else goto errstate;
        }
        else goto errstate;
    }

    t_dropzone *x = (t_dropzone *)pd_new(dropzone_class);
    x->x_outlet = outlet_new(&x->x_obj, &s_symbol);
    x->x_glist = (t_glist *)canvas_getcurrent();
    x->x_cv = canvas_getcurrent();
    x->x_zoom = x->x_cv->gl_zoom;
    x->x_edit = x->x_cv->gl_edit;
    x->x_drag_over = 0;
    x->x_proxy = dnd_proxy_new(x, gensym("__else_dnd_rcv"));
    x->x_width = w;
    x->x_height = h;
    x->x_active = 1;
    x->x_hover = 1;

    sprintf(x->x_tag_obj, "%pOBJ", x);
    sprintf(x->x_tag_in, "%pIN", x);
    sprintf(x->x_tag_out, "%pOUT", x);
    sprintf(x->x_tag_sel, "%pSEL", x);
    sprintf(x->x_tag_outline, "%pOUTLINE", x);
    sprintf(x->x_tag_hover, "%pHOVER", x);
    sprintf(x->x_tag_mb, ".x%lx.c.s%lx", (unsigned long)x->x_cv, (unsigned long)x);

    return (void *)x;

errstate:
    pd_error(x, "[dropzone]: improper args");
    return(NULL);
}

static void dropzone_free(t_dropzone *x) {
    outlet_free(x->x_outlet);
    t_dnd_proxy* p = x->x_proxy;
    p->p_dropzone = NULL;
    clock_delay(p->p_clock, 0);
}

void dropzone_setup(void) {
    dropzone_class = class_new(gensym("dropzone"),
                               (t_newmethod)dropzone_new,
                               (t_method)dropzone_free,
                               sizeof(t_dropzone),
                               0, A_GIMME, 0);

    class_addmethod(dropzone_class, (t_method)dropzone_zoom, gensym("zoom"), A_CANT, 0);
    class_addmethod(dropzone_class, (t_method)dropzone_dim, gensym("dim"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(dropzone_class, (t_method)dropzone_width, gensym("width"), A_FLOAT, 0);
    class_addmethod(dropzone_class, (t_method)dropzone_height, gensym("height"), A_FLOAT, 0);
    class_addmethod(dropzone_class, (t_method)dropzone_active, gensym("active"), A_FLOAT, 0);
    class_addmethod(dropzone_class, (t_method)dropzone_hover, gensym("hover"), A_FLOAT, 0);

    dropzone_widgetbehavior.w_getrectfn  = dropzone_getrect;
    dropzone_widgetbehavior.w_displacefn = dropzone_displace;
    dropzone_widgetbehavior.w_selectfn   = dropzone_select;
    dropzone_widgetbehavior.w_deletefn   = dropzone_delete;
    dropzone_widgetbehavior.w_visfn      = dropzone_vis;
    dropzone_widgetbehavior.w_clickfn    = NULL;
    class_setwidget(dropzone_class, &dropzone_widgetbehavior);

    dnd_proxy_class = class_new(0, 0, 0, sizeof(t_dnd_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(dnd_proxy_class, dnd_proxy_any);
    class_addmethod(dnd_proxy_class, (t_method)dnd_proxy_drag_over, gensym("_drag_over"), A_GIMME, 0);
    class_addmethod(dnd_proxy_class, (t_method)dnd_proxy_drag_leave, gensym("_drag_leave"), 0);
    class_addmethod(dnd_proxy_class, (t_method)dnd_proxy_drag_drop, gensym("_drag_drop"), A_GIMME, 0);

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
    "proc ::else_dnd::send_dnd_coordinates {mytoplevel x y} {\n"
    "    set tkcanvas [tkcanvas_name $mytoplevel]\n"
    "    set scrollregion [$tkcanvas cget -scrollregion]\n"
    "    set left_xview_pix [expr {([lindex [$tkcanvas xview] 0] * ([lindex $scrollregion 2] - [lindex $scrollregion 0])) + [lindex $scrollregion 0]}]\n"
    "    set top_yview_pix [expr {([lindex [$tkcanvas yview] 0] * ([lindex $scrollregion 3] - [lindex $scrollregion 1])) + [lindex $scrollregion 1]}]\n"
    "    set xrel [expr int($x - [winfo rootx $mytoplevel] + $left_xview_pix)]\n"
    "    set yrel [expr int($y - [winfo rooty $mytoplevel] + $top_yview_pix)]\n"
    "    pdsend \"__else_dnd_rcv _drag_over $mytoplevel $xrel $yrel \"\n"
    "}\n"
    "proc ::else_dnd::send_dnd_files {mytoplevel files} {\n"
    "    foreach file $files {\n"
    "        pdsend \"__else_dnd_rcv _drag_drop $mytoplevel 0 [::else_dnd::correct_spaces [file normalize $file]] \"\n"
    "    }\n"
    "}\n"
    "proc ::else_dnd::send_dnd_text {mytoplevel text} {\n"
    "        pdsend \"__else_dnd_rcv _drag_drop $mytoplevel 1 [::else_dnd::correct_spaces $text] \"\n"
    "}\n"
    "proc ::else_dnd::bind_dnd_canvas {mytoplevel} {\n"
    "    ::tkdnd::drop_target register $mytoplevel *\n"
    "    bind $mytoplevel <<DropPosition>> { ::else_dnd::send_dnd_coordinates %%W %%X %%Y}\n"
    "    bind $mytoplevel <<Drop:DND_Files>> { ::else_dnd::send_dnd_files %%W %%D }\n"
    "    bind $mytoplevel <<Drop:DND_Text>> { ::else_dnd::send_dnd_text %%W %%D }\n"
    "    bind $mytoplevel <<DropLeave>> { pdsend \"__else_dnd_rcv _drag_leave\"}\n"
    "}\n", dropzone_class->c_externdir->s_name);
}
