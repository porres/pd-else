// porres 2017-2020

#include "m_pd.h"
#include <stdlib.h>
#include <math.h>

static t_class *xgate_class;

#define OUTPUTLIMIT 512
#define HALF_PI (3.14159265358979323846 * 0.5)

typedef struct _xgate {
    t_object    x_obj;
    int         x_ch;
    int         x_n;
    int         x_lastch;
    int         x_n_outs;
    double      x_fade_n;
    float       x_sr_khz;
    int         x_active_ch[OUTPUTLIMIT];
    int         x_count[OUTPUTLIMIT];
    float      *x_outs[OUTPUTLIMIT];
    t_outlet   *x_out_status;
}t_xgate;

void xgate_float(t_xgate *x, t_floatarg f){
  x->x_ch = f < 0 ? 0 : f > x->x_n_outs ? x->x_n_outs : (int)f;
  if(x->x_ch != x->x_lastch){
      if(x->x_ch)
          x->x_active_ch[x->x_ch - 1] = 1;
      if(x->x_lastch)
          x->x_active_ch[x->x_lastch - 1] = 0;
      x->x_lastch = x->x_ch;
  }
}

static t_int *xgate_perform(t_int *w){
    t_xgate *x = (t_xgate *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    int i;
    for(i = 0; i < x->x_n_outs; i++)
        x->x_outs[i] = (t_float *)(w[3 + i]); // all outputs
    for(i = 0; i < x->x_n; i++){
        float input = in[i];
        for(int n = 0; n < x->x_n_outs; n++){
            if(x->x_active_ch[n] && x->x_count[n] < x->x_fade_n)
                x->x_count[n]++;
            else if(!x->x_active_ch[n] && x->x_count[n] > 0){
                x->x_count[n]--;
                if(x->x_count[n] == 0){
                    t_atom at[2];
                    SETFLOAT(at, n+1);
                    SETFLOAT(at+1, 0);
                    outlet_list(x->x_out_status, gensym("list"), 2, at);
                }
            }
            *x->x_outs[n]++ = input * sin((x->x_count[n] / x->x_fade_n) * HALF_PI);
        }
    }
    return(w+3+x->x_n_outs);
}

static void xgate_dsp(t_xgate *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    int size = x->x_n_outs + 2;
    t_int* sigvec = (t_int*)calloc(size, sizeof(t_int));
    sigvec[0] = (t_int)x;
    sigvec[1] = (t_int)sp[0]->s_vec; // in
    for(int i = 0; i < x->x_n_outs; i++) // outs
        sigvec[2+i] = (t_int)sp[1+i]->s_vec;
    dsp_addv(xgate_perform, size, (t_int*)sigvec);
    free(sigvec);
}

static void xgate_time(t_xgate *x, t_floatarg f){
    float ms = f < 0 ? 0 : f;
    double last_fade_n = x->x_fade_n;
    x->x_fade_n = x->x_sr_khz * ms + 1;
    for(int n = 0; n < x->x_n_outs; n++)
        if(x->x_count[n]) // adjust counters
            x->x_count[n] = x->x_count[n] / last_fade_n * x->x_fade_n;
}

static void *xgate_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_xgate *x = (t_xgate *)pd_new(xgate_class);
    x->x_sr_khz = sys_getsr() * 0.001;
    t_float ch = 1, ms = 0, init_channel = 0;
    int i;
    int argnum = 0;
    while(argc){
        if(argv -> a_type == A_FLOAT) { //if current argument is a float
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
    x->x_out_status = outlet_new(&x->x_obj, &s_list);
    ms = ms > 0 ? ms : 0;
    x->x_fade_n = x->x_sr_khz * ms + 1;
    x->x_lastch = 0;
    for(i = 0; i < OUTPUTLIMIT; i++){
        x->x_active_ch[i] = 0;
        x->x_count[i] = 0;
    }
    xgate_float(x, init_channel);
    return(x);
}

void xgate_tilde_setup(void) {
    xgate_class = class_new(gensym("xgate~"), (t_newmethod)xgate_new, 0,
        sizeof(t_xgate), CLASS_DEFAULT, A_GIMME, 0);
    class_addfloat(xgate_class, (t_method)xgate_float);
    class_addmethod(xgate_class, nullfn, gensym("signal"), 0);
    class_addmethod(xgate_class, (t_method)xgate_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(xgate_class, (t_method)xgate_time, gensym("time"), A_FLOAT, 0);
}
