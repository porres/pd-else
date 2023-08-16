// porres 2018

#include "m_pd.h"

typedef struct _ptouchin{
    t_object       x_ob;
    t_int          x_omni;
    t_float        x_ch;
    t_float        x_ch_in;
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

static void *ptouchin_new(t_symbol *s, t_floatarg f){
    s = NULL;
    t_ptouchin *x = (t_ptouchin *)pd_new(ptouchin_class);
    x->x_ptouch =  x->x_ready = x->x_key = 0;
    int ch = f < 0 ? 0 : f > 16 ? 16 : (int)f;
    x->x_omni = (ch == 0);
    x->x_ch = x->x_ch_in = ch;
    floatinlet_new((t_object *)x, &x->x_ch_in);
    outlet_new((t_object *)x, &s_float);
    x->x_chanout = outlet_new((t_object *)x, &s_float);
    return(x);
}

void setup_ptouch0x2ein(void){
    ptouchin_class = class_new(gensym("ptouch.in"), (t_newmethod)ptouchin_new,
        0, sizeof(t_ptouchin), 0, A_DEFFLOAT, 0);
    class_addfloat(ptouchin_class, ptouchin_float);
}
