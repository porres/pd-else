
#include "m_pd.h"
#include "mouse_gui.h"

typedef struct _mouse{
    t_object   x_ob;
}t_mouse;

static t_class *mouse_class;

static void mouse_any(void){ // dummy
}

static void mouse_doup(t_mouse *x, t_floatarg f){
    outlet_float(((t_object *)x)->ob_outlet, (int)f == 0);
}

static void mouse_free(t_mouse *x){
    hammergui_unbindmouse((t_pd *)x);
}

static void *mouse_new(void){
    t_mouse *x = (t_mouse *)pd_new(mouse_class);
    outlet_new((t_object *)x, &s_float);
    hammergui_bindmouse((t_pd *)x);
    return(x);
}

void mouse_setup(void){
    mouse_class = class_new(gensym("mouse"), (t_newmethod)mouse_new,
        (t_method)mouse_free, sizeof(t_mouse), CLASS_NOINLET, 0);
    class_addanything(mouse_class, mouse_any);
    class_addmethod(mouse_class, (t_method)mouse_doup, gensym("_up"), A_FLOAT, 0);
}
