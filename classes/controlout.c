// porres 2018

#include "m_pd.h"
#include <string.h>

typedef struct _controlout{
    t_object  x_ob;
    t_float   x_channel;
    t_float   x_n;
}t_controlout;

static t_class *controlout_class;

static void controlout_float(t_controlout *x, t_float f){
    if(f >= 0 && f <= 127){
        t_int channel = (int)x->x_channel;
        if(channel <= 0)
            channel = 1;
        if(x->x_n >= 0 && x->x_n <= 127){
            outlet_float(((t_object *)x)->ob_outlet, 176 + ((channel-1) & 0x0F));
            outlet_float(((t_object *)x)->ob_outlet, (t_int)x->x_n);
            outlet_float(((t_object *)x)->ob_outlet, (t_int)f);
        }
    }
}

static void *controlout_new(t_symbol *s, t_int ac, t_atom *av){
    t_controlout *x = (t_controlout *)pd_new(controlout_class);
    t_symbol *curarg = NULL;
    curarg = s; // get rid of warning
    t_float channel = 1;
    if(ac){
        while(ac > 0){
            if(av->a_type == A_FLOAT){
                channel = (t_int)atom_getfloatarg(0, ac, av);
                ac--, av++;
            }
            else
                goto errstate;
        }
    }
    x->x_channel = (channel > 0 ? channel : 1);
    floatinlet_new((t_object *)x, &x->x_n);
    floatinlet_new((t_object *)x, &x->x_channel);
    outlet_new((t_object *)x, &s_float);
    return(x);
    errstate:
        pd_error(x, "[controlout]: improper args");
        return NULL;
}

void controlout_setup(void){
    controlout_class = class_new(gensym("controlout"), (t_newmethod)controlout_new,
            0, sizeof(t_controlout), 0, A_GIMME, 0);
    class_addfloat(controlout_class, controlout_float);
}
