// porres 2018

#include "m_pd.h"

typedef struct _touchout{
    t_object  x_obj;
    t_float   x_ch;
}t_touchout;

static t_class *touchout_class;

static void touchout_float(t_touchout *x, t_float f){
    if(f >= 0 && f <= 127){
        t_int ch = (int)x->x_ch;
        if(ch < 1)
            ch = 1;
        outlet_float(((t_object *)x)->ob_outlet, 208 + ((ch-1) & 0x0F));
        outlet_float(((t_object *)x)->ob_outlet, (t_int)f);
    }
}

static void *touchout_new(t_floatarg f){
    t_touchout *x = (t_touchout *)pd_new(touchout_class);
    x->x_ch = f;
    floatinlet_new((t_object *)x, &x->x_ch);
    outlet_new((t_object *)x, &s_float);
    return(x);
}

void setup_touch0x2eout(void){
    touchout_class = class_new(gensym("touch.out"), (t_newmethod)touchout_new,
        0, sizeof(t_touchout), 0, A_DEFFLOAT, 0);
    class_addfloat(touchout_class, touchout_float);
}
