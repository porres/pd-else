#include <m_pd.h>
#include <g_canvas.h>

static t_class *closebang_class;

typedef struct _closebang{
    t_object x_obj;
}t_closebang;

static void closebang_closebang(t_closebang *x, t_floatarg action){
    if(action == LB_CLOSE)
        outlet_bang(x->x_obj.ob_outlet);
}

static void *closebang_new(void){
    t_closebang *x = (t_closebang *)pd_new(closebang_class);
    outlet_new(&x->x_obj, &s_bang);
    return(x);
}

void closebang_setup(void){
    closebang_class = class_new(gensym("closebang"), (t_newmethod)closebang_new,
        0, sizeof(t_closebang), CLASS_NOINLET, 0);
    class_addmethod(closebang_class, (t_method)closebang_closebang,
        gensym("loadbang"), A_DEFFLOAT, 0);
}
