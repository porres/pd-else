#include "m_pd.h"
#include "g_canvas.h"

static t_class *dir_class;

typedef struct dir{
    t_object x_obj;
    t_canvas * x_canvas;
}t_dir;

static void dir_bang(t_dir *x){
    outlet_symbol(x->x_obj.ob_outlet, canvas_getdir(x->x_canvas));
}

static void *dir_new(void){
    t_dir *x = (t_dir *)pd_new(dir_class);
    x->x_canvas =  canvas_getcurrent();
    outlet_new(&x->x_obj, &s_symbol);
    return (x);
}

void dir_setup(void){
    dir_class = class_new(gensym("dir"), (t_newmethod)dir_new,
                          0, sizeof(t_dir), 0, 0);
    class_addbang(dir_class, dir_bang);
}
