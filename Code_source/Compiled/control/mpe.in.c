// porres 2023

#include "m_pd.h"

enum{NOTEON, NOTEOFF, PRESSURE, BEND, SLIDE};

typedef struct _mpein{
    t_object       x_obj;
    int            x_ready;
    int            x_type;
    unsigned char  x_channel;
    unsigned char  x_byte1;
}t_mpein;

static t_class *mpein_class;

static void mpein_float(t_mpein *x, t_float f){ // raw MIDI
    if(f < 0 || f > 256)
        return;
    unsigned char val = (unsigned char)f;
    if(val & 0x80){ // (val > 128) is a channel message
        x->x_ready = 0;
        x->x_type = -1;
        unsigned char status = val & 0xF0;
        unsigned char ch = (val & 0x0F);
        if((status == 0x80) && (ch > 0) > 0x00){ // NOTE OFF
            x->x_type = NOTEOFF;
            x->x_channel = ch - 1;
        }
        else if((status == 0x90) && (ch > 0) > 0x00){ // NOTE ON
            x->x_type = NOTEON;
            x->x_channel = ch - 1;
        }
        else if((status == 0xB0) && (ch > 0) > 0x00){ // CC
            x->x_type = SLIDE;
            x->x_channel = ch - 1;
        }
        else if((status == 0xD0) && (ch > 0) > 0x00){ // PRESSURE
            x->x_ready = 1;
            x->x_type = PRESSURE;
            x->x_channel = ch - 1;
        }
        else if((status == 0xE0) && (ch > 0) > 0x00){ // BEND
            x->x_type = BEND;
            x->x_channel = ch - 1;
        }
    }
    else{ // < 128, byte value
        if(!x->x_ready){
            if(x->x_type == SLIDE && val == 74) // if CC#74, get ready
                x->x_ready = 1;
            else if(x->x_type >= 0){
                x->x_byte1 = val;
                x->x_ready = 1;
            }
        }
        else{ // it's ready
            if(x->x_type == NOTEON){
                t_atom at[4];
                SETFLOAT(at, x->x_channel);
                SETSYMBOL(at + 1, gensym("noteon"));
                SETFLOAT(at + 2, x->x_byte1);
                SETFLOAT(at + 3, val);
                outlet_list(((t_object *)x)->ob_outlet, &s_list, 4, at);
            }
            else if(x->x_type == NOTEOFF){
                t_atom at[4];
                SETFLOAT(at, x->x_channel);
                SETSYMBOL(at + 1, gensym("noteoff"));
                SETFLOAT(at + 2, x->x_byte1);
                SETFLOAT(at + 3, val);
                outlet_list(((t_object *)x)->ob_outlet, &s_list, 4, at);
            }
            else if(x->x_type == PRESSURE){
                t_atom at[3];
                SETFLOAT(at, x->x_channel);
                SETSYMBOL(at + 1, gensym("pressure"));
                SETFLOAT(at + 2, val);
                outlet_list(((t_object *)x)->ob_outlet, &s_list, 3, at);
            }
            else if(x->x_type == SLIDE){
                t_atom at[3];
                SETFLOAT(at, x->x_channel);
                SETSYMBOL(at + 1, gensym("slide"));
                SETFLOAT(at + 2, val);
                outlet_list(((t_object *)x)->ob_outlet, &s_list, 3, at);
            }
            else if(x->x_type == BEND){
                t_atom at[3];
                SETFLOAT(at, x->x_channel);
                SETSYMBOL(at + 1, gensym("bend"));
                SETFLOAT(at + 2, (val << 7) + x->x_byte1);
                outlet_list(((t_object *)x)->ob_outlet, &s_list, 3, at);
            }
            x->x_ready = 0;
            x->x_type = -1;
        }
    }
}

void *mpein_new(void){
    t_mpein *x = (t_mpein *)pd_new(mpein_class);
    x->x_ready = 0;
    x->x_type = -1;
    outlet_new((t_object *)x, &s_list);
    return(x);
}

void setup_mpe0x2ein(void){
    mpein_class = class_new(gensym("mpe.in"), (t_newmethod)mpein_new, 0, sizeof(t_mpein), 0, 0);
    class_addfloat(mpein_class, mpein_float);
}

