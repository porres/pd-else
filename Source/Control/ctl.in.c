// porres 2018-2024

#include <m_pd.h>

typedef struct _ctlin{
    t_object       x_obj;
    t_float        x_ch_in;
    t_float        x_ctl_in;
    t_int          x_ext;
    unsigned char  x_n;
    unsigned char  x_ready;
    unsigned char  x_control;
    unsigned char  x_channel;
    t_outlet      *x_val_out;
    t_outlet      *x_n_out;
    t_outlet      *x_chanout;
}t_ctlin;

static t_class *ctlin_class;

static void ctlin_float(t_ctlin *x, t_float f){
    if(f < 0 || f > 256){
        x->x_control = 0;
        return;
    }
    else{
        unsigned char val = (int)f;
        if(val & 0x80){ // message type > 128)
            x->x_ready = 0;
            if((x->x_control = ((val & 0xF0) == 0xB0))) // if control
                x->x_channel = (val & 0x0F) + 1; // get channel
        }
        else if(x->x_control && val < 128){
            if(x->x_ch_in <= 0){ // omni
                if(!x->x_ready){
                    x->x_n = val;
                    x->x_ready = 1;
                }
                else{ // ready
                    outlet_float(x->x_chanout, x->x_channel);
                    outlet_float(x->x_n_out, x->x_n);
                    outlet_float(x->x_val_out, val);
                    x->x_control = x->x_ready = 0;
                }
            }
            else if(x->x_ch_in == x->x_channel){
                if(!x->x_ready){
                    x->x_n = val;
                    x->x_ready = 1;
                }
                else{
                    if(x->x_ctl_in >= 0){
                        if(x->x_ctl_in == x->x_n){
                            outlet_float(x->x_chanout, x->x_channel);
                            outlet_float(x->x_n_out, x->x_n);
                            outlet_float(((t_object *)x)->ob_outlet, val);
                            x->x_control = x->x_ready = 0;
                        }
                    }
                    else{
                        outlet_float(x->x_chanout, x->x_channel);
                        outlet_float(x->x_n_out, x->x_n);
                        outlet_float(x->x_val_out, val);
                        x->x_control = x->x_ready = 0;
                    }
                }
            }
        }
        else
            x->x_control = x->x_ready = 0;
    }
}

static void ctlin_list(t_ctlin *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac || x->x_ext)
        return;
    int n = atom_getfloatarg(0, ac, av);
    int value = atom_getfloatarg(1, ac, av);
    int channel = atom_getfloatarg(2, ac, av);
    if(x->x_ch_in > 0 && x->x_ch_in != channel)
        return;
    if(x->x_ctl_in > 0 && x->x_ctl_in != n)
        return;
    outlet_float(x->x_chanout, channel);
    outlet_float(x->x_n_out, n);
    outlet_float(x->x_val_out, value);
}

static void ctlin_ext(t_ctlin *x, t_floatarg f){
    x->x_ext = f != 0;
}

static void ctlin_free(t_ctlin *x){
    pd_unbind(&x->x_obj.ob_pd, gensym("#ctlin"));
}

static void *ctlin_new(t_symbol *s, int ac, t_atom *av){
    t_ctlin *x = (t_ctlin *)pd_new(ctlin_class);
    t_symbol *curarg = NULL;
    curarg = s; // get rid of warning
    x->x_control = x->x_ready = x->x_n = 0;
    t_float channel = 0, ctl = -1;
    if(ac){
        if(atom_getsymbolarg(0, ac, av) == gensym("-ext")){
            x->x_ext = 1;
            ac--, av++;
        }
        if(ac){
            if(ac == 1)
                channel = atom_getint(av);
            else{
                ctl = atom_getint(av);
                ac--, av++;
                if(ac)
                    x->x_ch_in = atom_getint(av);
            }
        }
    }
    x->x_ctl_in = ctl < 0 ? 0 : ctl > 127 ? 127 : ctl;
    floatinlet_new((t_object *)x, &x->x_ctl_in);
    floatinlet_new((t_object *)x, &x->x_ch_in);
    x->x_val_out = outlet_new((t_object *)x, &s_float);
    x->x_n_out = outlet_new((t_object *)x, &s_float);
    x->x_chanout = outlet_new((t_object *)x, &s_float);
    pd_bind(&x->x_obj.ob_pd, gensym("#ctlin"));
    return(x);
}

void setup_ctl0x2ein(void){
    ctlin_class = class_new(gensym("ctl.in"), (t_newmethod)ctlin_new,
        (t_method)ctlin_free, sizeof(t_ctlin), 0, A_GIMME, 0);
    class_addfloat(ctlin_class, ctlin_float);
    class_addlist(ctlin_class, ctlin_list);
    class_addmethod(ctlin_class, (t_method)ctlin_ext, gensym("ext"), A_DEFFLOAT, 0);
}
