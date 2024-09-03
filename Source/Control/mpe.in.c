// porres 2023-2024

#include "m_pd.h"

enum{NOTEON, NOTEOFF, PRESSURE, BEND, SLIDE};

typedef struct _mpein{
    t_object       x_obj;
    int            x_ready;
    int            x_type;
    int            x_ext;
    t_int          x_port;
    t_float        x_port_in;
    unsigned char  x_channel;
    unsigned char  x_byte1;
    t_outlet      *x_portout;
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
            if(x->x_type == SLIDE){
                if(val == 74) // if CC#74, get ready
                    x->x_ready = 1;
            }
            else if(x->x_type >= 0){
                x->x_byte1 = val;
                x->x_ready = 1;
            }
        }
        else{ // it's ready
            if(x->x_port_in > 0 && x->x_port_in != x->x_port)
                goto ignore;
            outlet_float(x->x_portout, x->x_port);
            if(x->x_type == NOTEON){
                t_atom at[4];
                
                SETFLOAT(at, x->x_channel);
                SETFLOAT(at + 1, x->x_byte1);
                SETFLOAT(at + 2, val);
                SETFLOAT(at + 3, 0);
                outlet_list(((t_object *)x)->ob_outlet, &s_list, 4, at);
                
                SETSYMBOL(at + 1, gensym("noteon"));
                SETFLOAT(at + 2, x->x_byte1);
                SETFLOAT(at + 3, val);
                outlet_list(((t_object *)x)->ob_outlet, &s_list, 4, at);
            }
            else if(x->x_type == NOTEOFF){
                t_atom at[4];
                
                SETFLOAT(at, x->x_channel);
                SETFLOAT(at + 1, x->x_byte1);
                SETFLOAT(at + 2, 0);
                SETFLOAT(at + 3, val);
                outlet_list(((t_object *)x)->ob_outlet, &s_list, 4, at);
                
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
            ignore:
            x->x_ready = 0;
            x->x_type = -1;
        }
    }
}

static void mpein_list(t_mpein *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac || x->x_ext)
        return;
    x->x_port = atom_getfloatarg(1, ac, av) + 1;
    if(x->x_port_in > 0 && x->x_port_in != x->x_port)
        return;
    mpein_float(x, atom_getfloat(av));
    x->x_port = 0;
}

static void mpein_ext(t_mpein *x, t_floatarg f){
    x->x_ext = f != 0;
}

static void mpein_free(t_mpein *x){
    pd_unbind(&x->x_obj.ob_pd, gensym("#midiin"));
}

static void *mpein_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_mpein *x = (t_mpein *)pd_new(mpein_class);
    x->x_ready = x->x_ext = 0;
    x->x_type = -1;
    x->x_port_in = 0;
    if(ac > 0){
        if(av->a_type == A_SYMBOL && (atom_getsymbol(av) == gensym("-ext"))){
            x->x_ext = 1;
            ac--, av++;
        }
        if(ac)
            x->x_port_in = atom_getint(av);
    }
    floatinlet_new((t_object *)x, &x->x_port_in);
    outlet_new((t_object *)x, &s_list);
    x->x_portout = outlet_new((t_object *)x, &s_float);
    pd_bind (&x->x_obj.ob_pd, gensym("#midiin"));
    return(x);
}

void setup_mpe0x2ein(void){
    mpein_class = class_new(gensym("mpe.in"), (t_newmethod)mpein_new,
        (t_method)mpein_free, sizeof(t_mpein), 0, A_GIMME, 0);
    class_addfloat(mpein_class, mpein_float);
    class_addlist(mpein_class, mpein_list);
    class_addmethod(mpein_class, (t_method)mpein_ext, gensym("ext"), A_DEFFLOAT, 0);
}
