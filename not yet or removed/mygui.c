// Example GUI external with mouse wheel support
#include "m_pd.h"
#include "g_canvas.h"

typedef struct _mygui {
    t_object x_obj;
    t_glist *x_glist;
    int x_width, x_height;
    int x_x, x_y;  // position
    t_symbol *x_bindsym;  // unique symbol for this instance
    t_outlet *x_wheel_out;
} t_mygui;

static t_class *mygui_class;
t_widgetbehavior mygui_widgetbehavior;

// Wheel event handler
static void mygui_wheel(t_mygui *x, t_floatarg delta, t_floatarg horizontal) {
    // Output wheel data: delta and direction
    t_atom at[2];
    SETFLOAT(&at[0], delta);
    SETFLOAT(&at[1], horizontal);
    outlet_list(x->x_wheel_out, &s_list, 2, at);
}

// Create the GUI widget and bind wheel events
static void mygui_drawme(t_mygui *x, t_glist *glist, int firsttime) {
    if (firsttime) {
        // Create your GUI widget (example: canvas)
        sys_vgui("canvas %s -width %d -height %d -bg white\n",
                 x->x_bindsym->s_name, x->x_width, x->x_height);
        
        // Bind wheel events specifically to this widget
        sys_vgui("bind %s <<mouse_wheel_v>> {pdsend {%s _wheel %%D 0}}\n",
                 x->x_bindsym->s_name, x->x_bindsym->s_name);
        sys_vgui("bind %s <<mouse_wheel_h>> {pdsend {%s _wheel %%D 1}}\n",
                 x->x_bindsym->s_name, x->x_bindsym->s_name);
        
        // Position the widget
        sys_vgui("place %s -x %d -y %d\n",
                 x->x_bindsym->s_name, x->x_x, x->x_y);
    }
}

// Standard GUI callbacks
static void mygui_vis(t_gobj *z, t_glist *glist, int vis) {
    t_mygui *x = (t_mygui *)z;
    if (vis)
        mygui_drawme(x, glist, 1);
    else
        sys_vgui("destroy %s\n", x->x_bindsym->s_name);
}

static void mygui_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2) {
    t_mygui *x = (t_mygui *)z;
    *xp1 = x->x_x;
    *yp1 = x->x_y;
    *xp2 = x->x_x + x->x_width;
    *yp2 = x->x_y + x->x_height;
}

static void mygui_delete(t_gobj *z, t_glist *glist) {
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void mygui_displace(t_gobj *z, t_glist *glist, int dx, int dy) {
    t_mygui *x = (t_mygui *)z;
    x->x_x += dx;
    x->x_y += dy;
    sys_vgui("place %s -x %d -y %d\n", x->x_bindsym->s_name, x->x_x, x->x_y);
}

static void mygui_select(t_gobj *z, t_glist *glist, int state) {
    // Handle selection visual feedback
}

static void mygui_free(t_mygui *x) {
    // Cleanup: unbind and destroy widget
    pd_unbind((t_pd *)x, x->x_bindsym);
    sys_vgui("destroy %s\n", x->x_bindsym->s_name);
}

static void *mygui_new(t_floatarg w, t_floatarg h) {
    t_mygui *x = (t_mygui *)pd_new(mygui_class);
    
    x->x_glist = (t_glist *)canvas_getcurrent();
    x->x_width = (w > 0) ? (int)w : 100;
    x->x_height = (h > 0) ? (int)h : 100;
    x->x_x = x->x_y = 0;
    
    // Create unique symbol for this instance
    char buf[64];
    sprintf(buf, "mygui%lx", (unsigned long)x);
    x->x_bindsym = gensym(buf);
    
    // Bind this object to receive wheel messages
    pd_bind((t_pd *)x, x->x_bindsym);
    
    // Create outlet for wheel events
    x->x_wheel_out = outlet_new(&x->x_obj, &s_list);
    
    return (x);
}

void mygui_setup(void) {
    mygui_class = class_new(gensym("mygui"),
        (t_newmethod)mygui_new, (t_method)mygui_free,
        sizeof(t_mygui), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    
    // Standard GUI methods
    class_setwidget(mygui_class, &mygui_widgetbehavior);
    
    mygui_widgetbehavior.w_getrectfn  = mygui_getrect;
    mygui_widgetbehavior.w_displacefn = mygui_displace;
    mygui_widgetbehavior.w_selectfn   = mygui_select;
    mygui_widgetbehavior.w_activatefn = NULL;
    mygui_widgetbehavior.w_deletefn   = mygui_delete;
    mygui_widgetbehavior.w_visfn      = mygui_vis;
    mygui_widgetbehavior.w_clickfn    = NULL;
    
    // Add wheel method
    class_addmethod(mygui_class, (t_method)mygui_wheel,
                   gensym("_wheel"), A_FLOAT, A_FLOAT, 0);
    
    // Setup wheel events (reuse from your mouse code)
    sys_gui("event add <<mouse_wheel_v>> <MouseWheel>\n");
    sys_gui("event add <<mouse_wheel_h>> <Shift-MouseWheel>\n");
    sys_gui("event add <<mouse_wheel_v>> <Button-4>\n");
    sys_gui("event add <<mouse_wheel_v>> <Button-5>\n");
    sys_gui("event add <<mouse_wheel_h>> <Shift-Button-4>\n");
    sys_gui("event add <<mouse_wheel_h>> <Shift-Button-5>\n");
}
