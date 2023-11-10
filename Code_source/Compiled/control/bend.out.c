// porres 2018

#include "m_pd.h"
#include "s_stuff.h"

typedef struct _bendout{
    t_object  x_ob;
    t_float   x_channel;
    t_int     x_raw;
}t_bendout;

static t_class *bendout_class;

void bendout_midiout(int value){
#ifdef USEAPI_ALSA
  if(sys_midiapi == API_ALSA)
      sys_alsa_putmidibyte(0, value);
  else
#endif
    sys_putmidibyte(0, value);
}

static void bendout_output(t_bendout *x, t_float f){
    outlet_float(((t_object *)x)->ob_outlet, f);
    bendout_midiout(f);
}

static void bendout_float(t_bendout *x, t_float f){
    t_int bend;
    t_int channel = (int)x->x_channel;
    if(channel <= 0)
        channel = 1;
    if(x->x_raw)
        bend = (t_int)f;
    else
        bend = (t_int)(f * 8191) + 8192;
    if(bend >= 0 && bend <= 16383){
        bendout_output(x, 224 + ((channel-1) & 0x0F));
        bendout_output(x, bend & 0x7F);
        bendout_output(x,  bend >> 7);
    }
}

static void *bendout_new(t_symbol *s, int ac, t_atom *av){
    t_bendout *x = (t_bendout *)pd_new(bendout_class);
    t_symbol *curarg = s; // get rid of warning
    floatinlet_new((t_object *)x, &x->x_channel);
    outlet_new((t_object *)x, &s_float);
    t_float channel = 1;
    int floatarg = 0;
    if(ac){
        while(ac > 0){
            if(av->a_type == A_FLOAT){
                floatarg = 1;
                channel = (t_int)atom_getfloatarg(0, ac, av);
                ac--, av++;
            }
            else if(av->a_type == A_SYMBOL && !floatarg){
                curarg = atom_getsymbolarg(0, ac, av);
                if(curarg == gensym("-raw")){
                    x->x_raw = 1;
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
    }
    x->x_channel = (channel > 0 ? channel : 1);
    return(x);
errstate:
    pd_error(x, "[bend.out]: improper args");
    return(NULL);
}

void setup_bend0x2eout(void){
    bendout_class = class_new(gensym("bend.out"), (t_newmethod)bendout_new,
            0, sizeof(t_bendout), 0, A_GIMME, 0);
    class_addfloat(bendout_class, bendout_float);
}
