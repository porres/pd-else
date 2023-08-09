// porres 2017-2020

#include "m_pd.h"
#include <stdlib.h>
#include <math.h>

static t_class *xgatemc_class;

#define OUTPUTLIMIT 512
#define HALF_PI (3.14159265358979323846 * 0.5)

typedef struct _xgatemc{
    t_object    x_obj;
    int         x_nchs;
    int         x_n;
    int         x_channel;
    int         x_lastchannel;
    int         x_n_outs;
    double      x_n_fade; // fade length in samples
    float       x_sr_khz;
    int         x_active_channel[OUTPUTLIMIT];
    int         x_count[OUTPUTLIMIT];
    float       *x_outs[OUTPUTLIMIT];
}t_xgatemc;

void xgatemc_float(t_xgatemc *x, t_floatarg ch){
  ch = (int)ch > x->x_n_outs ? x->x_n_outs : (int)ch;
  x->x_channel = ch < 0 ? 0 : ch;
  if(x->x_channel != x->x_lastchannel){
      if(x->x_channel)
          x->x_active_channel[x->x_channel - 1] = 1;
      if(x->x_lastchannel)
          x->x_active_channel[x->x_lastchannel - 1] = 0;
      x->x_lastchannel = x->x_channel;
  }
}

static t_int *xgatemc_perform(t_int *w){
    t_xgatemc *x = (t_xgatemc *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    for(int i = 0; i < x->x_n_outs; i++)
        x->x_outs[i] = (t_float *)(w[3+i]); // outputs
    for(int i = 0; i < x->x_n * x->x_nchs; i++){
        t_float input = in[i];
        for(int n = 0; n < x->x_n_outs; n++){
            if(x->x_active_channel[n] && x->x_count[n] < x->x_n_fade)
                x->x_count[n]++;
            else if(!x->x_active_channel[n] && x->x_count[n] > 0){
                x->x_count[n]--;
            }
            double amp = sin(x->x_count[n] / x->x_n_fade * HALF_PI);
            *x->x_outs[n]++ = input * amp;
        }
    }
    return(w+3+x->x_n_outs);
}

static void xgatemc_dsp(t_xgatemc *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    x->x_nchs = sp[0]->s_nchans;
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    int size = x->x_n_outs + 2;
    t_int *sigvec = (t_int*)calloc(size, sizeof(t_int));
    sigvec[0] = (t_int)x; // object
    sigvec[1] = (t_int)sp[0]->s_vec; // in
    for(int i = 0; i < x->x_n_outs; i++){ // outs
        sigvec[2+i] = (t_int)sp[1+i]->s_vec;
        signal_setmultiout(&sp[1+i], x->x_nchs);
    }
    dsp_addv(xgatemc_perform, size, (t_int*)sigvec);
    free(sigvec);
}

static void xgatemc_time(t_xgatemc *x, t_floatarg ms){
    int i;
    double last_fade_in_samps = x->x_n_fade;
    ms = ms < 0 ? 0 : ms;
    x->x_n_fade = x->x_sr_khz * ms + 1;
    for(i = 0; i < x->x_n_outs; i++)
        if(x->x_count[i]) // adjust counters
            x->x_count[i] = x->x_count[i] / last_fade_in_samps * x->x_n_fade;
}

static void *xgatemc_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_xgatemc *x = (t_xgatemc *)pd_new(xgatemc_class);
    x->x_sr_khz = sys_getsr() * 0.001;
    t_float ch = 1, ms = 0, init_channel = 0;
    int i;
    int argnum = 0;
    while(argc){
        if(argv -> a_type == A_FLOAT){ // if current argument is a float
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum){
                case 0:
                    ch = argval;
                    break;
                case 1:
                    ms = argval;
                    break;
                case 2:
                    init_channel = argval;
                default:
                    break;
            };
        }
        argnum++;
        argc--;
        argv++;
    };
    x->x_n_outs = ch < 1 ? 1 : ch;
    if(x->x_n_outs > OUTPUTLIMIT)
        x->x_n_outs = OUTPUTLIMIT;
    for(i = 0; i < x->x_n_outs; i++)
        outlet_new(&x->x_obj, gensym("signal"));
    ms = ms > 0 ? ms : 0;
    x->x_n_fade = x->x_sr_khz * ms + 1;
    x->x_lastchannel = 0;
    for(i = 0; i < OUTPUTLIMIT; i++){
        x->x_active_channel[i] = 0;
        x->x_count[i] = 0;
    }
    xgatemc_float(x, init_channel);
    return(x);
}

void setup_xgate0x2emc_tilde(void){
    xgatemc_class = class_new(gensym("xgate.mc~"), (t_newmethod)xgatemc_new, 0,
        sizeof(t_xgatemc), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addfloat(xgatemc_class, (t_method)xgatemc_float);
    class_addmethod(xgatemc_class, nullfn, gensym("signal"), 0);
    class_addmethod(xgatemc_class, (t_method)xgatemc_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(xgatemc_class, (t_method)xgatemc_time, gensym("time"), A_FLOAT, 0);
}
