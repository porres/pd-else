#include <stdio.h>
#include <string.h>
#include <m_pd.h>
#include <g_canvas.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

static t_class *name_class;

typedef struct _name{
    t_object x_obj;
    t_symbol *x_name;
}t_name;

static void name_bang(t_name *x){
    outlet_symbol(x->x_obj.ob_outlet, x->x_name);
}

static void *name_new(t_floatarg f){
    t_name *x = (t_name *)pd_new(name_class);
    t_glist *glist = (t_glist *)canvas_getcurrent();
    t_canvas *canvas = (t_canvas *)glist_getcanvas(glist);
    int depth = f < 0 ? 0 : (int)f;
    while(depth && canvas){
        canvas = canvas->gl_owner;
        depth--;
    }
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING, ".x%lx.c", (long unsigned int)canvas);
    x->x_name = gensym(buf);
    outlet_new(&x->x_obj, &s_symbol);
    return(x);
}

void name_setup(void){
    name_class = class_new(gensym("name"), (t_newmethod)name_new,
        NULL, sizeof(t_name), 0, A_DEFFLOAT, 0);
    class_addbang(name_class, (t_method)name_bang);
}
