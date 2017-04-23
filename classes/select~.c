

#include "m_pd.h"
#include <math.h>
#include <string.h>

#define HALF_PI (M_PI * 0.5)

static t_class *select_class;

#define INPUTLIMIT 256
#define LINEAR 0
#define EPOWER 1

typedef struct _ip { // [0] is 1st inlet!!!
  int active[INPUTLIMIT]; 
  int counter[INPUTLIMIT];
  double fade[INPUTLIMIT];
  float *in[INPUTLIMIT];
} t_ip;

typedef struct _select {
  t_object x_obj;
  int channel;
  int lastchannel;
  int actuallastchannel;
  int ninlets;
  int fade_in_samps;
  int fadetype;
  int lastfadetype;
  float sr_khz;
  t_ip ip;
} t_select;

void select_float(t_select *x, t_floatarg ch){
  ch = (int)ch;
  ch = ch > x->ninlets ? x->ninlets : ch;
  ch = ch < 0 ? 0 : ch;
  if(ch != x->lastchannel){
    x->channel = ch;
    if(x->channel)
        x->ip.active[x->channel - 1] = 1;
    if(x->lastchannel) {
        x->ip.active[x->lastchannel - 1] = 0;
        }
    x->actuallastchannel = x->lastchannel;
    x->lastchannel = x->channel;
  }
}

static t_int *select_perform(t_int *w){
    int i;
    t_select *x = (t_select *)(w[1]);
    int n = (int)(w[2]);
    for(i = 0; i < x->ninlets; i++)
        x->ip.in[i] = (t_float *)(w[3 + i]);
    float *out = (t_float *)(w[3 + x->ninlets]);
    while (n--)
    {
        float sum = 0;
        for(i = 0; i < x->ninlets; i++) {
            if(x->ip.active[i] && x->ip.counter[i] < x->fade_in_samps)
                x->ip.counter[i]++;
            else if (!x->ip.active[i] && x->ip.counter[i] > 0)
                x->ip.counter[i]--;
            x->ip.fade[i] = x->ip.counter[i] / (double)x->fade_in_samps;
            if(x->fadetype == EPOWER)
                sum += *x->ip.in[i]++ * sin(x->ip.fade[i] * HALF_PI);
            else
                sum += *x->ip.in[i]++ * x->ip.fade[i];
            }
        *out++ = sum;
    }
    return (w + 4 + x->ninlets);
}

static void select_dsp(t_select *x, t_signal **sp) {
    x->sr_khz = sp[0]->s_sr * 0.001;
    long i;
    t_int **sigvec;
    int count = x->ninlets + 3;
    sigvec  = (t_int **) calloc(count, sizeof(t_int *));
    for(i = 0; i < count; i++)
        sigvec[i] = (t_int *) calloc(sizeof(t_int), 1); // init sigvec
    sigvec[0] = (t_int *)x; // 1st => object
    sigvec[1] = (t_int *)sp[0]->s_n; // 2nd => block (n)
    for(i = 2; i < count; i++) // ins/out
        sigvec[i] = (t_int *)sp[i-2]->s_vec;
    dsp_addv(select_perform, count, (t_int *) sigvec);
    free(sigvec);
}

static void select_time(t_select *x, t_floatarg time) {
    int i;
    time = time < 0 ? 0 : time;
    x->fade_in_samps = (int)(x->sr_khz * time) + 1; // no. of ticks to reach specified fade time
    for(i = 0; i < x->ninlets; i++){ // adjustcounters
        if(x->ip.counter[i])
            x->ip.counter[i] = x->ip.fade[i] * (double)x->fade_in_samps;
    }
}

void select_lin(t_select *x) { // BUGGED!!!!!!
    int i;
    if(x->lastfadetype != LINEAR){ // change to linear
        for(i = 0; i < x->ninlets; i++) {
            double ep = x->ip.fade[i];
            ep = ep < 0 ? 0 : ep > 1 ? : 1;
            ep = sin(ep * HALF_PI);
            int oldcounter = x->ip.counter[i];
            x->ip.counter[i] = ep * x->fade_in_samps;
            x->ip.fade[i] = x->ip.counter[i] / (double)x->fade_in_samps; // ???
        }
        x->lastfadetype = x->fadetype = LINEAR;
    }
}

void select_ep(t_select *x) {
    int i;
    if(x->lastfadetype != EPOWER){ // change to equal power
        for(i = 0; i < x->ninlets; i++) {
            int oldcounter = x->ip.counter[i];
            double aepower = (acos(x->ip.fade[i]) + HALF_PI) / HALF_PI;
            aepower = 2 - aepower;   // ??? - but does the trick (???????????)
            aepower = aepower < 0 ? 0 : aepower > 1 ? 1 : aepower;
            x->ip.counter[i] = aepower * x->fade_in_samps;
        }
        x->lastfadetype = x->fadetype = EPOWER;
    }
}

static void *select_new(t_symbol *s, int argc, t_atom *argv) {
    t_select *x = (t_select *)pd_new(select_class);
    x->sr_khz = sys_getsr() * 0.001;
    t_float ch = 1, ms = 0, fade_mode = EPOWER, init_channel = 0;
    int i;
    int argnum = 0;
    while(argc > 0){
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
            argnum++;
            argc--;
            argv++;
        }
        else if (argv -> a_type == A_SYMBOL){
            t_symbol *curarg = atom_getsymbolarg(0, argc, argv);
            if(strcmp(curarg->s_name, "-lin")==0) {
                fade_mode = LINEAR;
                argc -= 1;
                argv += 1;
                }
            else{
                goto errstate;
            };
        }
    };
    x->fadetype = x->lastfadetype = fade_mode;
    x->ninlets = ch < 1 ? 1 : ch;
    if(x->ninlets > INPUTLIMIT)
        x->ninlets = INPUTLIMIT;
    for(i = 0; i < x->ninlets - 1; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, gensym("signal"));
    x->lastchannel = x->actuallastchannel = 0;
    ms = ms > 0 ? ms : 0;
    x->fade_in_samps = (int)(x->sr_khz * ms) + 1; // no. of samples to crossfade
    x->channel = init_channel;
    for(i = 0; i < INPUTLIMIT; i++){
        x->ip.active[i] = 0;
        x->ip.counter[i] = 0;
        x->ip.fade[i] = 0;
    }
    select_float(x, x->channel);
    return (x);
    errstate:
        pd_error(x, "select~: improper args");
        return NULL;
}

void select_tilde_setup(void) {
    select_class = class_new(gensym("select~"), (t_newmethod)select_new, 0,
                             sizeof(t_select), CLASS_DEFAULT, A_GIMME, 0);
    class_addfloat(select_class, (t_method)select_float);
    class_addmethod(select_class, nullfn, gensym("signal"), 0);
    class_addmethod(select_class, (t_method)select_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(select_class, (t_method)select_time, gensym("time"), A_FLOAT, (t_atomtype) 0);
    class_addmethod(select_class, (t_method)select_lin, gensym("lin"), 0);
    class_addmethod(select_class, (t_method)select_ep, gensym("ep"), 0);
}
