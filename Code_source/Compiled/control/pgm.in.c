// porres 2018-2024

#include "m_pd.h"

typedef struct _pgmin{
    t_object       x_obj;
    t_int          x_ext, x_pgm, x_channel;
    t_float        x_ch_in; // float input channel
    t_outlet      *x_chanout;
}t_pgmin;

static t_class *pgmin_class;

static void pgmin_float(t_pgmin *x, t_float f){
    if(f < 0 || f > 256){ // out of range
        x->x_pgm = 0;
        return;
    }
    else{ // in range
        unsigned char val = (unsigned char)f;
        if(val & 0x80){ // message type > 128)
            if((x->x_pgm = ((val & 0xF0) == 0xC0))) // is pgm change
                x->x_channel = (val & 0x0F) + 1; // get channel
        }
        else if(x->x_pgm && val < 128){ // output value
            if(x->x_ch_in <= 0 || x->x_ch_in > 16){ // omni
                outlet_float(x->x_chanout, x->x_channel);
                outlet_float(((t_object *)x)->ob_outlet, val);
            }
            else if(x->x_channel == (t_int)x->x_ch_in)
                outlet_float(((t_object *)x)->ob_outlet, val);
            x->x_pgm = 0;
        }
        else
            x->x_pgm = 0;
    }
}

static void pgmin_list(t_pgmin *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac || x->x_ext)
        return;
    int pgm = atom_getfloatarg(0, ac, av);
    int channel = atom_getfloatarg(1, ac, av);
    if(x->x_ch_in > 0 && x->x_ch_in != channel)
        return;
    outlet_float(x->x_chanout, channel);
    outlet_float(((t_object *)x)->ob_outlet, pgm);
}

static void pgmin_ext(t_pgmin *x, t_floatarg f){
    x->x_ext = f != 0;
}

void *pgmin_free(t_pgmin *x){
    outlet_free(x->x_chanout);
    pd_unbind(&x->x_obj.ob_pd, gensym("#pgmin"));
    return(void *)x;
}

static void *pgmin_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_pgmin *x = (t_pgmin *)pd_new(pgmin_class);
    x->x_pgm = 0;
    x->x_ch_in = 0;
    x->x_ext = 0;
    if(ac > 0){
        if(av->a_type == A_SYMBOL && (atom_getsymbol(av) == gensym("-ext"))){
            x->x_ext = 1;
            ac--, av++;
        }
        if(ac)
            x->x_ch_in = atom_getint(av);
    }
    floatinlet_new((t_object *)x, &x->x_ch_in);
    outlet_new((t_object *)x, &s_float);
    x->x_chanout = outlet_new((t_object *)x, &s_float);
    pd_bind(&x->x_obj.ob_pd, gensym("#pgmin"));
    return(x);
}

void setup_pgm0x2ein(void){
    pgmin_class = class_new(gensym("pgm.in"), (t_newmethod)pgmin_new,
        (t_method)pgmin_free, sizeof(t_pgmin), 0, A_GIMME, 0);
    class_addfloat(pgmin_class, pgmin_float);
    class_addlist(pgmin_class, pgmin_list);
    class_addmethod(pgmin_class, (t_method)pgmin_ext, gensym("ext"), A_DEFFLOAT, 0);
}
