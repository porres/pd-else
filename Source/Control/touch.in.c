// porres 2018

#include <m_pd.h>

typedef struct _touchin{
    t_object       x_obj;
    t_float        x_ch_in;
    t_int          x_ext;
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
        unsigned char val = (int)f;
        if(val & 0x80){ // message type > 128)
            x->x_ready = 0;
             if((x->x_atouch = ((val & 0xF0) == 0xD0))) // if channel atouch
                x->x_channel = (val & 0x0F) + 1; // get channel
        }
        else if(x->x_atouch && val < 128){
            if(x->x_ch_in <= 0){ // omni
                outlet_float(x->x_chanout, x->x_channel);
                outlet_float(((t_object *)x)->ob_outlet, val);
            }
            else if(x->x_ch_in == x->x_channel)
                outlet_float(((t_object *)x)->ob_outlet, val);
        }
        else
            x->x_atouch = x->x_ready = 0;
    }
}

static void touchin_list(t_touchin *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac || x->x_ext)
        return;
    int touch = atom_getfloatarg(0, ac, av);
    int channel = atom_getfloatarg(1, ac, av);
    if(x->x_ch_in > 0 && x->x_ch_in != channel)
        return;
    outlet_float(x->x_chanout, channel);
    outlet_float(((t_object *)x)->ob_outlet, touch);
}

static void touchin_ext(t_touchin *x, t_floatarg f){
    x->x_ext = f != 0;
}

static void touchin_free(t_touchin *x){
    pd_unbind(&x->x_obj.ob_pd, gensym("#touchin"));
}

static void *touchin_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_touchin *x = (t_touchin *)pd_new(touchin_class);
    x->x_atouch = x->x_ch_in = x->x_ready = x->x_pressure = 0;
    if(ac){
        if(atom_getsymbolarg(0, ac, av) == gensym("-ext")){
            x->x_ext = 1;
            ac--, av++;
        }
        x->x_ch_in = atom_getint(av);
    }
    floatinlet_new((t_object *)x, &x->x_ch_in);
    outlet_new((t_object *)x, &s_float);
    x->x_chanout = outlet_new((t_object *)x, &s_float);
    pd_bind(&x->x_obj.ob_pd, gensym("#touchin"));
    return(x);
}

void setup_touch0x2ein(void){
    touchin_class = class_new(gensym("touch.in"), (t_newmethod)touchin_new,
        (t_method)touchin_free, sizeof(t_touchin), 0, A_GIMME, 0);
    class_addfloat(touchin_class, touchin_float);
    class_addlist(touchin_class, touchin_list);
    class_addmethod(touchin_class, (t_method)touchin_ext, gensym("ext"), A_DEFFLOAT, 0);
}
