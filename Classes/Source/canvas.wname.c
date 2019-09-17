
#include "m_pd.h"
#include <math.h>
#include "g_canvas.h"

static t_class *canvas_wname_class;

typedef struct canvas_wname{
    t_object x_obj;
    t_symbol *x_wname;
}t_canvas_wname;

static void canvas_wname_bang(t_canvas_wname *x){
    outlet_symbol(x->x_obj.ob_outlet, x->x_wname);
}

static void *canvas_wname_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_canvas_wname *x = (t_canvas_wname *)pd_new(canvas_wname_class);
    t_canvas *canvas = canvas_getcurrent();
    outlet_new(&x->x_obj, &s_);
    int depth = 0;
    if(argc && argv->a_type == A_FLOAT){
        float f = argv->a_w.w_float;
        depth = f < 0 ? 0 : (int)f;
    }
    while(depth && canvas){
        canvas = canvas->gl_owner;
        depth--;
    }
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING, ".x%lx", (long unsigned int)canvas);
    x->x_wname = gensym(buf);
    return(void *)x;
}

void setup_canvas0x2ewname(void){
    canvas_wname_class = class_new(gensym("canvas.wname"), (t_newmethod)canvas_wname_new,
        0, sizeof(t_canvas_wname), 0, A_GIMME,0);
    class_addbang(canvas_wname_class, canvas_wname_bang);
}
