// porres 2018

#include "m_pd.h"
#include <string.h>

typedef struct _programin{
    t_object       x_ob;
    t_int          x_omni;
    t_float        x_ch;
    t_float        x_ch_in;
    unsigned char  x_pgm;
    unsigned char  x_channel;
    t_outlet      *x_chanout;
}t_programin;

static t_class *programin_class;

static void programin_float(t_programin *x, t_float f){
    if(f < 0 || f > 256){
        x->x_pgm = 0;
        return;
    }
    else{
        t_int ch = (t_int)x->x_ch_in;
        if(ch != x->x_ch && ch >= 0 && ch <= 16)
            x->x_omni = ((x->x_ch = (t_int)ch) == 0);
        unsigned char val = (int)f;
        if(val & 0x80){ // message type > 128)
            if((x->x_pgm = ((val & 0xF0) == 0xC0))) // if pgm change
                x->x_channel = (val & 0x0F) + 1; // get channel
        }
        else if(x->x_pgm && val < 128){ // output value
            if(x->x_omni){
                outlet_float(x->x_chanout, x->x_channel);
                outlet_float(((t_object *)x)->ob_outlet, val);
            }
            else if(x->x_ch == x->x_channel)
                outlet_float(((t_object *)x)->ob_outlet, val);
            x->x_pgm = 0;
        }
        else
            x->x_pgm = 0;
    }
}

static void *programin_new(t_floatarg f){
    t_programin *x = (t_programin *)pd_new(programin_class);
    x->x_pgm = 0;
    t_int channel = (t_int)f;
    if(channel < 0)
        channel = 0;
    if(channel > 16)
        channel = 16;
    x->x_omni = (channel == 0);
    x->x_ch = x->x_ch_in = channel;
    floatinlet_new((t_object *)x, &x->x_ch_in);
    outlet_new((t_object *)x, &s_float);
    x->x_chanout = outlet_new((t_object *)x, &s_float);
    return(x);
}

void programin_setup(void){
    programin_class = class_new(gensym("programin"), (t_newmethod)programin_new,
        0, sizeof(t_programin), 0, A_DEFFLOAT, 0);
    class_addfloat(programin_class, programin_float);
}
