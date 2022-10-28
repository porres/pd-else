// [dss~]: "Dinamic Stochastic Synthesis" based on Xenakis' GenDyn stuff

#include "m_pd.h"
#include <math.h>
#include "random.h"

#define MAX_N 128

static t_class* dss_class;

typedef struct _dss{
    t_object       x_obj;
    t_random_state x_rstate;
    int            x_id;
    int            x_n;
    double         x_minf;
    double         x_maxf;
    int            x_adist;
    double         x_ampp;
    double         x_maxamp;
    int            x_ddist;
    double         x_durp;
    double         x_maxdur;
    int            x_interp;
// internal
    double         x_phase;
    int            x_i;           // index
    double         x_amp;
    double         x_dur;
    double         x_nextamp;
    double         x_phase_step;
    double         x_amps[MAX_N];
    double         x_durs[MAX_N];
    double         x_i_sr;
}t_dss;

static inline double dss_fold(double in, double min, double max){
    if(in >= min && in <= max)
        return(in);
    double range = 2.0 * fabs((double)(min - max));
    return(fabs(remainder(in - min, range)) + min);
}

static void dss_seed(t_dss *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
    uint32_t *s1 = &x->x_rstate.s1, *s2 = &x->x_rstate.s2, *s3 = &x->x_rstate.s3;
    for(int i = 0; i < MAX_N; i++){
        x->x_amps[i] = (double)(random_frand(s1, s2, s3));
        x->x_durs[i] = (double)(random_frand(s1, s2, s3)) * 0.5 + 0.5;
    }
    x->x_i = 0;
}

static double dss_dist(int d, double p, double f){
    double temp, c = 0;
    switch(d){
        case 0: // Uniform / linear:
            return(2*f - 1.0);
        case 1: // CAUCHY
            // X has a*tan((z-0.5)*pi)
            // went back to first principles of the Cauchy distribution and re-integrated with a
            // normalisation constant
            // choice of 10 here is such that f=0.95 gives about 0.35 for temp, could go with 2 to make it finer
            c = atan(10.0 * p);  // PERHAPS CHANGE TO a=1/a;
            // incorrect- missed out divisor of pi in norm temp = a*tan(c*(2*pi*f - 1));
            temp = (1 / p) * tan(c * (2 * f - 1));
            // Cauchy distribution, C is precalculated
            // printf("cauchy f %f c %f temp %f out %f \n",f,  c, temp, temp/10);
            return(temp * 0.1); // (temp+100) / 200;
        case 2 : // Logist:
            // X has -(log((1-z)/z)+b)/a which is not very usable as is
            c = 0.5 + (0.499 * p);
            c = log((1 - c) / c);
            // remap into range of valid inputs to avoid infinities in the log
            // f= ((f-0.5)*0.499*a)+0.5;
            f = ((f - 0.5) * 0.998 * p) + 0.5; // [0,1]->[0.001,0.999]; squashed around midpoint 0.5 by a
            // Xenakis calls this the LOGIST map, it's from the range [0,1] to [inf,0] where 0.5->1
            // than take natural log. to avoid infinities in practise I take [0,1] -> [0.001,0.999]->[6.9,-6.9]
            // an interesting property is that 0.5-e is the reciprocal of 0.5+e under (1-f)/f
            // and hence the logs are the negative of each other
            temp = log((1 - f) / f) / c;// n range [-1,1]
            // X also had two constants in his- I don't bother
            return(temp); // a*0.5*(temp+1.0);    //to [0,1]
        case 3: // Hyperbcos:
            // X original a*log(tan(z*pi/2)) which is [0,1]->[0,pi/2]->[0,inf]->[-inf,inf]
            // unmanageable in this pure form
            c = tan(1.5692255 * p); // tan(0.999*a*pi*0.5);        //[0, 636.6] maximum range
            temp = tan(1.5692255 * p * f) / c; //[0,1]->[0,1]
            temp = log(temp * 0.999 + 0.001) * (-0.1447648); // multiplier same as /(-6.9077553); //[0,1]->[0,1]
            return(2 * temp - 1.0);
        case 4: // Arcsine:
             // X original a/2*(1-sin((0.5-z)*pi)) aha almost a better behaved one though [0,1]->[2,0]->[a,0]
            c = sin(1.5707963 * p);  // sin(pi*0.5*a);    //a as scaling factor of domain of sine input to use
            temp = sin(M_PI * (f - 0.5) * p) / c; //[-1,1] which is what I need
            return(temp);
        case 5: // Expon:
            // X original -(log(1-z))/a [0,1]-> [1,0]-> [0,-inf]->[0,inf]
            c = log(1.0 - (0.999 * p));
            temp = log(1.0 - (f * 0.999 * p)) / c;
            return(2 * temp - 1.0);
        case 6: // Sinus:
            // X original a*sin(smp * 2*pi/44100 * b) ie depends on a second oscillator's value-
            // hmmm, plug this in as a I guess, will automatically accept control rate inputs then!
            return(2.f * p - 1.f);
        default:
            break;
    }
    return(2*f - 1.0);
}

static void dss_n(t_dss* x, t_floatarg f){
    x->x_n = f < 1 ? 1 : f > MAX_N ? MAX_N : (int)f;
}

static void dss_minfreq(t_dss* x, t_floatarg f){
    x->x_minf = f < 0.000001 ? 0.000001 : f > 22050 ? 22050 : f;
}

static void dss_maxfreq(t_dss* x, t_floatarg f){
    x->x_maxf = f < 0.000001 ? 0.000001 : f > 22050 ? 22050 : f;
}

static void dss_ampdist(t_dss* x, t_floatarg f){
    x->x_adist = f < 0 ? 0 : f > 6 ? 6 : (int)f;
}

static void dss_ampp(t_dss* x, t_floatarg f){
    x->x_ampp = f < 0.0001 ? 0.0001 : f > 1 ? 1 : f;
}

static void dss_ampscale(t_dss* x, t_floatarg f){
    x->x_maxamp =  f < 0 ? 0 : f > 1 ? 1 : f;
}

static void dss_durdist(t_dss* x, t_floatarg f){
    x->x_ddist = f < 0 ? 0 : f > 6 ? 6 : (int)f;
}

static void dss_durp(t_dss* x, t_floatarg f){
    x->x_durp =  f < 0.0001 ? 0.0001 : f > 1 ? 1 : f;
}

static void dss_durscale(t_dss* x, t_floatarg f){
    x->x_maxdur =  f < 0 ? 0 : f > 1 ? 1 : f;
}

static void dss_interp(t_dss* x, t_floatarg f){
    x->x_interp =  (f != 0);
}

static inline double dss_rand(t_dss* x){
    uint32_t *s1 = &x->x_rstate.s1, *s2 = &x->x_rstate.s2, *s3 = &x->x_rstate.s3;
    return((double)(random_frand(s1, s2, s3)) * 0.5 + 0.5);
}

static t_int* dss_perform(t_int* w){
    t_dss* x = (t_dss*)(w[1]);
    t_float* out = (t_float*)(w[2]);
    int block = (int)(w[3]);
    double phase = x->x_phase;
    double amp = x->x_amp;
    double dur = x->x_dur;
    double nextamp = x->x_nextamp;
    double phase_step = x->x_phase_step;
    for(int i = 0; i < block; ++i){ // ++i ????
// phase gives proportion for linear interpolation automatically
        if(phase >= 1){ // get new value
            phase -= 1;
            amp = x->x_amps[x->x_i], dur = x->x_durs[x->x_i]; // get values
//            post("dur = %f", (float)dur);
            double r = dss_dist(x->x_adist, x->x_ampp, dss_rand(x)) * x->x_maxamp;
            x->x_amps[x->x_i] = dss_fold(amp + r, -1.0, 1.0); // update amp
            r = dss_dist(x->x_ddist, x->x_durp, dss_rand(x)) * x->x_maxdur;
//            post("r = %f", (float)r);
            x->x_durs[x->x_i] = dss_fold(dur + r, 0.0, 1.0); // update dur
            x->x_i = (x->x_i + 1) % x->x_n; // next index
//            post("x->x_durs[x->x_i] = %f", (float)x->x_durs[x->x_i]);
            nextamp = x->x_amps[x->x_i]; // next index's amp
        }
        phase_step = (x->x_minf + ((x->x_maxf - x->x_minf) * dur)) * x->x_i_sr;
        phase_step *= x->x_n; // if there are 12 points, multiply speed by 12
        double output = amp;
        if(x->x_interp) // linear interpolation
            output = ((1.0 - phase) * amp) + (phase * nextamp);
        phase += phase_step;
        out[i] = output;
    }
    x->x_phase = phase;
    x->x_amp = amp;
    x->x_dur = dur;
    x->x_nextamp = nextamp;
    x->x_phase_step = phase_step;
    return(w+4);
}

static void dss_dsp(t_dss* x, t_signal** sp){
    x->x_i_sr = 1 / sys_getsr();
    dsp_add(dss_perform, 3, x, sp[0]->s_vec, (t_int)sp[0]->s_n);
}

static void* dss_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    ac = 0;
    av = NULL;
    t_dss* x = (t_dss*)pd_new(dss_class);
// default parameters
    x->x_n = 12;
    x->x_minf = 220, x->x_maxf = 440;
    x->x_adist = x->x_ddist = 0;
    x->x_ampp = x->x_durp = 0.5;
    x->x_maxamp = x->x_maxdur = 0.5;
// internal
    x->x_phase = 1;
    x->x_interp = 1;
    x->x_phase_step = 0;
    x->x_i_sr = 1 / sys_getsr();
    x->x_id = random_get_id();
    dss_seed(x, s, 0, NULL);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void dss_tilde_setup(void){
    dss_class = class_new(gensym("dss~"), (t_newmethod)dss_new, 0,
        sizeof(t_dss), 0, A_GIMME, 0);
    class_addmethod(dss_class, (t_method)dss_n, gensym("n"), A_FLOAT, 0);
    class_addmethod(dss_class, (t_method)dss_minfreq, gensym("minf"), A_FLOAT, 0);
    class_addmethod(dss_class, (t_method)dss_maxfreq, gensym("maxf"), A_FLOAT, 0);
//    class_addmethod(dss_class, (t_method)dss_rangef, gensym("rangef"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(dss_class, (t_method)dss_ampp, gensym("ampp"), A_FLOAT, 0);
    class_addmethod(dss_class, (t_method)dss_durp, gensym("durp"), A_FLOAT, 0);
    class_addmethod(dss_class, (t_method)dss_ampscale, gensym("amp"), A_FLOAT, 0);
    class_addmethod(dss_class, (t_method)dss_durscale, gensym("dur"), A_FLOAT, 0);
    class_addmethod(dss_class, (t_method)dss_ampdist, gensym("ampd"), A_FLOAT, 0);
    class_addmethod(dss_class, (t_method)dss_durdist, gensym("durd"), A_FLOAT, 0);
    class_addmethod(dss_class, (t_method)dss_interp, gensym("interp"), A_FLOAT, 0);
    class_addmethod(dss_class, (t_method)dss_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(dss_class, (t_method)dss_dsp, gensym("dsp"), A_CANT, 0);
}
