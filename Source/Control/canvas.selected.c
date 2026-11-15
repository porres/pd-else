// porres

#include <m_pd.h>
#include <g_canvas.h>

static t_class *canvas_selected_class;

typedef struct canvas_selected{
    t_object    x_obj;
    t_canvas *x_canvas;
}t_canvas_selected;

static void canvas_selected_bang(t_canvas_selected *x){
    t_canvas *c = x->x_canvas;
    int selected = c && c->gl_owner
        && glist_isselected(c->gl_owner, &c->gl_gobj);
    outlet_float(x->x_obj.ob_outlet, selected);
}

static void *canvas_selected_new(void){
    t_canvas_selected *x = (t_canvas_selected *)pd_new(canvas_selected_class);
    x->x_canvas = canvas_getcurrent();
    outlet_new(&x->x_obj, &s_float);
    return(void *)x;
}

void setup_canvas0x2eselected(void){
    canvas_selected_class = class_new(gensym("canvas.selected"), (t_newmethod)canvas_selected_new, 0, sizeof(t_canvas_selected), 0,0);
    class_addbang(canvas_selected_class, canvas_selected_bang);
}
