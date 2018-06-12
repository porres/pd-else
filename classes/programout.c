// porres 2018

#include "m_pd.h"
#include <string.h>

typedef struct _programout{
    t_object  x_ob;
    t_float   x_channel;
    t_int     x_raw;
}t_programout;

static t_class *programout_class;

static void programout_float(t_programout *x, t_float f){
    if(f >= 0 && f <= 127){
        t_int channel = (int)x->x_channel;
        if(channel <= 0)
            channel = 1;
        outlet_float(((t_object *)x)->ob_outlet, 192 + ((channel-1) & 0x0F));
        outlet_float(((t_object *)x)->ob_outlet, (t_int)f);
    }
}

static void *programout_new(t_symbol *s, t_int ac, t_atom *av){
    t_programout *x = (t_programout *)pd_new(programout_class);
    t_symbol *curarg = NULL;
    curarg = s; // get rid of warning
    floatinlet_new((t_object *)x, &x->x_channel);
    outlet_new((t_object *)x, &s_float);
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
    return(x);
    errstate:
        pd_error(x, "[programout]: improper args");
        return NULL;
}

void programout_setup(void){
    programout_class = class_new(gensym("programout"), (t_newmethod)programout_new,
            0, sizeof(t_programout), 0, A_GIMME, 0);
    class_addfloat(programout_class, programout_float);
}
