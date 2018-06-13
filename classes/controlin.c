// porres 2018

#include "m_pd.h"
#include <string.h>

typedef struct _controlin{
    t_object       x_ob;
    t_int          x_omni;
    t_float        x_ch;
    t_float        x_ch_in;
    unsigned char  x_n;
    unsigned char  x_ready;
    unsigned char  x_control;
    unsigned char  x_channel;
    t_outlet      *x_chanout;
    t_outlet      *x_n_out;
}t_controlin;

static t_class *controlin_class;

static void controlin_float(t_controlin *x, t_float f){
    if(f < 0 || f > 256){
        x->x_control = 0;
        return;
    }
    else{
        t_int ch = (t_int)x->x_ch_in;
        if(ch != x->x_ch && ch >= 0 && ch <= 16)
            x->x_omni = ((x->x_ch = (t_int)ch) == 0);
        unsigned char val = (int)f;
        if(val & 0x80){ // message type > 128)
            x->x_ready = 0;
            if((x->x_control = ((val & 0xF0) == 0xB0))) // if control
                x->x_channel = (val & 0x0F) + 1; // get channel
        }
        else if(x->x_control && val < 128){
            if(x->x_omni){
                if(!x->x_ready){
                    x->x_n = val;
                    x->x_ready = 1;
                }
                else{ // ready
                    outlet_float(x->x_chanout, x->x_channel);
                    outlet_float(x->x_n_out, x->x_n);
                    outlet_float(((t_object *)x)->ob_outlet, val);
                    x->x_control = x->x_ready = 0;
                }
            }
            else if(x->x_ch == x->x_channel){
                if(!x->x_ready){
                    x->x_n = val;
                    x->x_ready = 1;
                }
                else{
                    outlet_float(x->x_n_out, x->x_n);
                    outlet_float(((t_object *)x)->ob_outlet, val);
                    x->x_control = x->x_ready = 0;
                }
            }
        }
        else
            x->x_control = x->x_ready = 0;
    }
}

static void *controlin_new(t_symbol *s, t_int ac, t_atom *av){
    t_controlin *x = (t_controlin *)pd_new(controlin_class);
    t_symbol *curarg = NULL;
    curarg = s; // get rid of warning
    x->x_control = x->x_ready = x->x_n = 0;
    t_float channel = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            channel = (t_int)atom_getfloatarg(0, ac, av);
            ac--, av++;
        }
        else
            goto errstate;
    }
    if(channel < 0)
        channel = 0;
    if(channel > 16)
        channel = 16;
    x->x_omni = (channel == 0);
    x->x_ch = x->x_ch_in = channel;
    floatinlet_new((t_object *)x, &x->x_ch_in);
    outlet_new((t_object *)x, &s_float);
    x->x_n_out = outlet_new((t_object *)x, &s_float);
    x->x_chanout = outlet_new((t_object *)x, &s_float);
    return(x);
    errstate:
        pd_error(x, "[controlin]: improper args");
        return NULL;
}

void controlin_setup(void){
    controlin_class = class_new(gensym("controlin"), (t_newmethod)controlin_new,
        0, sizeof(t_controlin), 0, A_GIMME, 0);
    class_addfloat(controlin_class, controlin_float);
}
