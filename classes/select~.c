

#include "m_pd.h"
#include <math.h>
#include <string.h>

#define HALF_PI (M_PI * 0.5)

static t_class *select_class;

#define INPUTLIMIT 256
#define LINEAR 0
#define EPOWER 1
#define TIME_UNITS_MS (32.*441.) // ????????????????????????????

typedef struct _ip { // keeps track of each signal input
  int active[INPUTLIMIT]; 
  int counter[INPUTLIMIT];
  double timeoff[INPUTLIMIT];
  float fade[INPUTLIMIT];
  float *in[INPUTLIMIT];
} t_ip;

typedef struct _select {
  t_object x_obj;
  int channel;
  int lastchannel;
  int actuallastchannel;
  int ninlets;
  int fadetime;
  double changetime;
  int fadecount;
  int fadeticks;
  int fadetype;
  int lastfadetype;
  int fadealert; 
  float sr_khz;
  t_ip ip;
} t_select;

void select_float(t_select *x, t_floatarg f){
  f = (int)f;
  f = f > x->ninlets ? x->ninlets : f;
  f = f < 0 ? 0 : f;
  if(f != x->lastchannel){
    if(f == x->actuallastchannel)
        x->fadecount = x->fadeticks - x->fadecount;
    else
        x->fadecount = 0;
        x->channel = f;
    if(x->channel)
        x->ip.active[x->channel - 1] = 1;
    if(x->lastchannel) {
        x->ip.active[x->lastchannel - 1] = 0;
        x->ip.timeoff[x->lastchannel - 1] = clock_getlogicaltime();
        }
    x->actuallastchannel = x->lastchannel;
    x->lastchannel = x->channel;
  }
}


static double epower(double rate) {
 double tmp;
 if(rate < 0)
  rate = 0;
 if(rate > 0.999)
  rate = 0.999;
 rate *= HALF_PI;
 tmp = cos(rate - HALF_PI);
 tmp = tmp < 0 ? 0 : tmp;
 tmp = tmp > 1 ? 1 : tmp;
 return tmp;
}

static void updatefades(t_select *x){
    int i;
    for(i = 0; i < x->ninlets; i++){
        if(!x->ip.counter[i])
            x->ip.fade[i] = 0;
        if(x->ip.active[i] && x->ip.counter[i] < x->fadeticks){
            if(x->ip.counter[i])
                x->ip.fade[i] = x->ip.counter[i] / (float)x->fadeticks;
            x->ip.counter[i]++;
        }
        else if (!x->ip.active[i] && x->ip.counter[i] > 0){
            x->ip.fade[i] = x->ip.counter[i] / (float)x->fadeticks;
            x->ip.counter[i]--;
        }
    }
}

static t_int *select_perform(t_int *w){
    int i, j;
    t_select *x = (t_select *)(w[1]);
    int n = (int)(w[2]);
    for(i = 0; i < x->ninlets; i++)
        x->ip.in[i] = (t_float *)(w[3 + i]);
    float *out = (t_float *)(w[3 + x->ninlets]);
    while (n--) {
        float sum = 0;
        updatefades(x);
        for(i = 0; i < x->ninlets; i++)
            if(x->ip.fade[i]) {
                if(x->fadetype == EPOWER)
                    sum += *x->ip.in[i]++ * epower(x->ip.fade[i]);
                else
                    sum += *x->ip.in[i]++ * x->ip.fade[i];
            }
        *out++ = sum;
    }
    for(i = 0; i < x->ninlets; i++){ // check which input feeds oughtta be "switch~"ed off
        if(!x->ip.active[i])
            if(clock_gettimesince(x->ip.timeoff[i]) > x->fadetime && x->ip.timeoff[i]){
                x->ip.timeoff[i] = 0;
                x->ip.fade[i] = 0;
            }
    }
    return (w + 4 + x->ninlets);
}

static void select_dsp(t_select *x, t_signal **sp) {
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
    int i, shorter;
    time = time < 1 ? 1 : time;
    shorter = (time < x->fadetime);
    x->fadetime = (int)time;
    x->fadeticks = (int)(x->sr_khz * time); // no. of ticks to reach specified fade time
    for(i = 0; i < x->ninlets; i++){ // shortcheck
        if(shorter && x->ip.timeoff[i]) // correct active timeoffs for new x->fadeticks
            x->ip.timeoff[i] = clock_getlogicaltime() - ((x->fadeticks - x->ip.counter[i]) / x->sr_khz - 1) * TIME_UNITS_MS;
    }
    for(i = 0; i < x->ninlets; i++){ // adjustcounters
        if(x->ip.counter[i])
            x->ip.counter[i] = x->ip.fade[i] * (float)x->fadeticks;
    }
}

// JUNTAR COM MODE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
static double aepower(double ep) // convert from equal to linear
{ //    double answer = (atan(2*ep*ep - 1) + 0.785398) / 1.5866; // ???
    double answer = (acos(ep) + HALF_PI) / HALF_PI;
    answer = 2 - answer;   // ??? - but does the trick
    answer = answer < 0 ? 0 : answer;
    answer = answer > 1 ? 1 : answer;
    return answer;
}

void select_mode(t_select *x, t_floatarg mode) {
    int i;
    if(mode == 1 && x->lastfadetype != 1){ // change to equal power
        for(i = 0; i < x->ninlets; i++) {
            float fade = x->ip.fade[i];
            int oldcounter = x->ip.counter[i];
            x->ip.counter[i] = aepower(fade) * x->fadeticks;
            x->ip.fade[i] = epower(x->ip.counter[i]/ (float)x->fadeticks); // ???
        }
        x->lastfadetype = x->fadetype = EPOWER;
    }
    else if(mode == 0 && x->lastfadetype != 0){ // change to linear
        for(i = 0; i < x->ninlets; i++) {
            float fade = x->ip.fade[i];
            int oldcounter = x->ip.counter[i];
            x->ip.counter[i] = epower(fade) * x->fadeticks;
            x->ip.fade[i] = x->ip.counter[i]/ (float)x->fadeticks; // ???
        }
        x->lastfadetype = x->fadetype = LINEAR;
    }
}
// JUNTAR COM AEPOWER!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

static void *select_new(t_symbol *s, int argc, t_atom *argv) {
    t_select *x = (t_select *)pd_new(select_class);
    x->sr_khz = sys_getsr() * 0.001;
    t_float ch = 1, ms = 1, mode = 1;
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
                default:
                    break;
            };
            argnum++;
            argc--;
            argv++;
        }
    };
    x->fadetype = x->lastfadetype = EPOWER;
    x->ninlets = ch < 1 ? 1 : ch;
    if(x->ninlets > INPUTLIMIT)
        x->ninlets = INPUTLIMIT;
    x->fadetime = ms >= 1 ? ms : 1; // what happens with ms = 0???
    for(i = 0; i < x->ninlets - 1; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, gensym("signal"));
    x->channel = 0; x->lastchannel = x->actuallastchannel = 0;
    x->fadecount = 0;
    x->fadeticks = (int)(x->sr_khz * x->fadetime); // no. of ticks to reach specified fade 'rate'
    x->fadealert = 0;
    for(i = 0; i < INPUTLIMIT; i++){
        x->ip.active[i] = 0;
        x->ip.counter[i] = 0;
        x->ip.timeoff[i] = 0;
        x->ip.fade[i] = 0;
    }
    return (x);
}

void select_tilde_setup(void) {
    select_class = class_new(gensym("select~"), (t_newmethod)select_new, 0,
                             sizeof(t_select), CLASS_DEFAULT, A_GIMME, 0);
    class_addfloat(select_class, (t_method)select_float);
    class_addmethod(select_class, nullfn, gensym("signal"), 0);
    class_addmethod(select_class, (t_method)select_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(select_class, (t_method)select_time, gensym("time"), A_FLOAT, (t_atomtype) 0);
    class_addmethod(select_class, (t_method)select_mode, gensym("mode"), A_FLOAT, (t_atomtype) 0);
}
