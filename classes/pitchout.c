// porres 2018

#include "m_pd.h"
#include <string.h>

typedef struct _pitchout{
    t_object  x_ob;
    t_float   x_channel;
    t_float   x_flag;
    t_float   x_velocity;
    t_int     x_pitch;
    t_int     x_rel;
}t_pitchout;

static t_class *pitchout_class;

static void pitchout_float(t_pitchout *x, t_float f){
    int pitch = (int)f;
    if(pitch >= 0 && pitch <= 127){
        int channel = (int)x->x_channel;
        if(channel <= 0)
            channel = 1;
        int velocity = (int)x->x_velocity;
        if(velocity < 0)
            velocity = 0;
        if(velocity > 127)
            velocity = 127;
        velocity &= 0x7F;
        if(x->x_rel){
            int status = ((int)x->x_flag ? 0x90 : 0x80);
            outlet_float(((t_object *)x)->ob_outlet, status + ((channel-1) & 0x0F));
            outlet_float(((t_object *)x)->ob_outlet, pitch);
            outlet_float(((t_object *)x)->ob_outlet, velocity);
        }
        else{
            int status = (velocity != 0 ? 0x90 : 0x80);
            outlet_float(((t_object *)x)->ob_outlet, status + ((channel-1) & 0x0F));
            outlet_float(((t_object *)x)->ob_outlet, pitch);
            outlet_float(((t_object *)x)->ob_outlet, velocity);
        }
    }
}

static void *pitchout_new(t_symbol *s, t_int ac, t_atom *av){
    t_pitchout *x = (t_pitchout *)pd_new(pitchout_class);
    t_symbol *curarg = s; // get rid of warning
    float channel = 1;
    if(ac){
        while(ac > 0){
            if(av->a_type == A_FLOAT){
                channel = (t_int)atom_getfloatarg(0, ac, av);
                ac--;
                av++;
            }
            else if(av->a_type == A_SYMBOL){
                curarg = atom_getsymbolarg(0, ac, av);
                if(!strcmp(curarg->s_name, "-rel")){
                    x->x_rel = 1;
                    ac--;
                    av++;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
    }
    floatinlet_new((t_object *)x, &x->x_velocity);
    if(x->x_rel)
        floatinlet_new((t_object *)x, &x->x_flag);
    floatinlet_new((t_object *)x, &x->x_channel);
    outlet_new((t_object *)x, &s_float);
    x->x_channel = (channel > 0 ? channel : 1);
    x->x_flag = 0;
    x->x_velocity = 0;
    x->x_pitch = -1;
    return(x);
    errstate:
        pd_error(x, "[pitchout]: improper args");
        return NULL;
}

void pitchout_setup(void){
    pitchout_class = class_new(gensym("pitchout"), (t_newmethod)pitchout_new, 0,
        sizeof(t_pitchout), 0, A_GIMME, 0);
    class_addfloat(pitchout_class, pitchout_float);
}
