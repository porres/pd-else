// porres 2018

#include "m_pd.h"
//#include <string.h>

typedef struct _touchin{
    t_object       x_ob;
    t_int          x_omni;
    t_float        x_ch;
    t_float        x_ch_in;
    unsigned char  x_pressure;
    unsigned char  x_ready;
    unsigned char  x_atouch;
    unsigned char  x_channel;
    t_outlet      *x_chanout;
    t_outlet      *x_pressout;
}t_touchin;

static t_class *touchin_class;

static void touchin_float(t_touchin *x, t_float f){
    if(f < 0 || f > 256){
        x->x_atouch = 0;
        return;
    }
    else{
        t_int ch = (t_int)x->x_ch_in;
        if(ch != x->x_ch && ch >= 0 && ch <= 16)
            x->x_omni = ((x->x_ch = (t_int)ch) == 0);
        unsigned char val = (int)f;
        if(val & 0x80){ // message type > 128)
            x->x_ready = 0;
             if((x->x_atouch = ((val & 0xF0) == 0xD0))) // if channel atouch
                x->x_channel = (val & 0x0F) + 1; // get channel
        }
        else if(x->x_atouch && val < 128){
            if(x->x_omni){
                outlet_float(x->x_chanout, x->x_channel);
                outlet_float(((t_object *)x)->ob_outlet, val);
            }
            else if(x->x_ch == x->x_channel)
                outlet_float(((t_object *)x)->ob_outlet, val);
        }
        else
            x->x_atouch = x->x_ready = 0;
    }
}

static void *touchin_new(t_symbol *s, t_floatarg f){
    s = NULL;
    t_touchin *x = (t_touchin *)pd_new(touchin_class);
    x->x_atouch =  x->x_ready = x->x_pressure = 0;
    int ch = f < 0 ? 0 : f > 16 ? 16 : (int)f;
    x->x_omni = (ch == 0);
    x->x_ch = x->x_ch_in = ch;
    floatinlet_new((t_object *)x, &x->x_ch_in);
    outlet_new((t_object *)x, &s_float);
    x->x_chanout = outlet_new((t_object *)x, &s_float);
    return(x);
}

void setup_touch0x2ein(void){
    touchin_class = class_new(gensym("touch.in"), (t_newmethod)touchin_new,
        0, sizeof(t_touchin), 0, A_DEFFLOAT, 0);
    class_addfloat(touchin_class, touchin_float);
}
