// porres 2018

#include "m_pd.h"

typedef struct _ptouchin{
    t_object       x_obj;
    t_int          x_omni;
    t_float        x_ch;
    t_float        x_ch_in;
    t_int          x_ext;
    unsigned char  x_key;
    unsigned char  x_ready;
    unsigned char  x_ptouch;
    unsigned char  x_channel;
    t_outlet      *x_chanout;
}t_ptouchin;

static t_class *ptouchin_class;

static void ptouchin_float(t_ptouchin *x, t_float f){
    if(f < 0 || f > 256){
        x->x_ptouch = 0;
        return;
    }
    else{
        t_int ch = (t_int)x->x_ch_in;
        if(ch != x->x_ch && ch >= 0 && ch <= 16)
            x->x_omni = ((x->x_ch = (t_int)ch) == 0);
        unsigned char val = (int)f;
        if(val & 0x80){ // message type > 128)
            x->x_ready = 0;
            if((x->x_ptouch = ((val & 0xF0) == 0xA0))) // if poly aptouch
                x->x_channel = (val & 0x0F) + 1; // get ch
        }
        else if(x->x_ptouch && val < 128){
            if(x->x_omni){
                outlet_float(x->x_chanout, x->x_channel);
                if(!x->x_ready){
                    x->x_key = val;
                    x->x_ready = 1;
                }
                else{ // ready
                    t_atom at[2];
                    SETFLOAT(at, x->x_key);
                    SETFLOAT(at+1, val);
                    outlet_list(((t_object *)x)->ob_outlet, &s_list, 2, at);
                    x->x_ptouch = x->x_ready = 0;
                }
            }
            else if(x->x_ch == x->x_channel){
                if(!x->x_ready){
                    x->x_key = val;
                    x->x_ready = 1;
                }
                else{
                    t_atom at[2];
                    SETFLOAT(at, x->x_key);
                    SETFLOAT(at+1, val);
                    outlet_list(((t_object *)x)->ob_outlet, &s_list, 2, at);
                    x->x_ptouch = x->x_ready = 0;
                }
            }
        }
        else
            x->x_ptouch = x->x_ready = 0;
    }
}

static void ptouchin_list(t_ptouchin *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        return;
    if(!x->x_ext)
        ptouchin_float(x, atom_getfloat(av));
}

static void ptouchin_ext(t_ptouchin *x, t_floatarg f){
    x->x_ext = f != 0;
}

static void ptouchin_free(t_ptouchin *x){
    pd_unbind(&x->x_obj.ob_pd, gensym("#midiin"));
}

static void *ptouchin_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_ptouchin *x = (t_ptouchin *)pd_new(ptouchin_class);
    x->x_ptouch =  x->x_ready = x->x_key = 0;
    int ch = 0;
    if(ac){
        if(atom_getsymbolarg(0, ac, av) == gensym("-ext")){
            x->x_ext = 1;
            ac--, av++;
        }
        ch = (t_int)atom_getintarg(0, ac, av);
    }
    ch = ch < 0 ? 0 : ch > 16 ? 16 : ch;
    x->x_omni = (ch == 0);
    x->x_ch = x->x_ch_in = ch;
    floatinlet_new((t_object *)x, &x->x_ch_in);
    outlet_new((t_object *)x, &s_float);
    x->x_chanout = outlet_new((t_object *)x, &s_float);
    pd_bind(&x->x_obj.ob_pd, gensym("#midiin"));
    return(x);
}

void setup_ptouch0x2ein(void){
    ptouchin_class = class_new(gensym("ptouch.in"), (t_newmethod)ptouchin_new,
        (t_method)ptouchin_free, sizeof(t_ptouchin), 0, A_GIMME, 0);
    class_addfloat(ptouchin_class, ptouchin_float);
    class_addlist(ptouchin_class, ptouchin_list);
    class_addmethod(ptouchin_class, (t_method)ptouchin_ext, gensym("ext"), A_DEFFLOAT, 0);
}
