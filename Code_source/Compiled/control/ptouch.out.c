// porres 2018

#include "m_pd.h"
#include "s_stuff.h"

typedef struct _ptouchout{
    t_object  x_obj;
    t_float   x_ch;
    t_float   x_pressure;
    t_int     x_ext;
}t_ptouchout;

static t_class *ptouchout_class;

void ptouchout_midiout(int value){
#ifdef USEAPI_ALSA
  if(sys_midiapi == API_ALSA)
      sys_alsa_putmidibyte(0, value);
  else
#endif
    sys_putmidibyte(0, value);
}

static void ptouchout_output(t_ptouchout *x, t_float f){
    outlet_float(((t_object *)x)->ob_outlet, f);
    if(!x->x_ext)
        ptouchout_midiout(f);
}

static void ptouch_ext(t_ptouchout *x, t_floatarg f){
    x->x_ext = f != 0;
}

static void ptouchout_float(t_ptouchout *x, t_float f){
    if(f >= 0 && f <= 127){
        t_int channel = (int)x->x_ch;
        if(channel <= 0)
            channel = 1;
        if(x->x_pressure >= 0 && x->x_pressure <= 127){
            ptouchout_output(x, 160 + ((channel-1) & 0x0F));
            ptouchout_output(x, (t_int)f);
            ptouchout_output(x, (t_int)x->x_pressure);
        }
    }
}

static void *ptouchout_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_ptouchout *x = (t_ptouchout *)pd_new(ptouchout_class);
    t_float channel = 1;
    x->x_ext = 0;
    if(ac){
        if(atom_getsymbol(av) == gensym("-ext")){
            x->x_ext = 1;
            ac--, av++;
        }
        if(ac && av->a_type == A_FLOAT)
            channel = (t_int)atom_getfloatarg(0, ac, av);
    }
    x->x_ch = (channel > 0 ? channel : 1);
    floatinlet_new((t_object *)x, &x->x_pressure);
    floatinlet_new((t_object *)x, &x->x_ch);
    outlet_new((t_object *)x, &s_float);
    return(x);
}

void setup_ptouch0x2eout(void){
    ptouchout_class = class_new(gensym("ptouch.out"), (t_newmethod)ptouchout_new,
        0, sizeof(t_ptouchout), 0, A_GIMME, 0);
    class_addfloat(ptouchout_class, ptouchout_float);
    class_addmethod(ptouchout_class, (t_method)ptouch_ext, gensym("ext"), A_FLOAT, 0);
}
