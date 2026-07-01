// [gendyn~]: "Dynamic Stochastic Synthesis" based/inspired on Xenakis' GenDyn stuff
// code by porres, 2022-2026

#include <m_pd.h>
#include <stdlib.h>
#include <random.h>
#include <buffer.h>
#include "magic.h"

#define _USE_MATH_DEFINES
#include <math.h>

#define MAX_N   128
#define MAXLEN  1024

static t_class* gendyn_class;

typedef struct _gendyn{
    t_object       x_obj;          // the object
    t_random_state x_rstate;       // random generator state
    int            x_id;           // random id value
    int            x_n;            // number of points in the period
    double         x_bw;           // bandwidth around center frequency
    int            x_cents;        // flag for bandwidth in cents
    int            x_ad;           // amplitude distribution number
    int            x_fd;           // frequency distribution number
    double         x_ap;           // amplitude distribution paramter
    double         x_fp;           // frequency distribution paramter
    double         x_astep;        // maximum amplitude step
    double         x_fstep;        // maximum frequency step
    int            x_interp;       // interpolation mode
//    double         x_ratio;        // converted ratio value from cents
    
    float         *x_freq_list;    // center frequency
    t_float       *x_last_cf;      // last center frequency
    double        *x_phase;        // running phase
    double        *x_phase_step;   // running frequency's phase step
    int           *x_i;            // point's index
    double        *x_freq;         // current point's frequency
    double        *x_amp;          // current point's amplitude
    double        *x_nextamp;      // next point's amplitude
    int           *x_changed;
    
    double        *x_amps;         // array for points' amplitudes
    double        *x_freqs;        // array for points' frequencies
    
    double         x_i_sr;         // inverse sample rate
    int            x_nblock;
    t_int          x_sig1;
    t_int          x_list_size;
    int            x_nchans;
    t_glist       *x_glist;
}t_gendyn;

static inline double gendyn_fold(double in, double min, double max){
    if(in >= min && in <= max)
        return(in);
    double range = 2.0 * fabs((double)(min - max));
    return(fabs(remainder(in - min, range)) + min);
}

static double gendyn_rand(t_gendyn *x, int d, double p){
    uint32_t *s1 = &x->x_rstate.s1, *s2 = &x->x_rstate.s2, *s3 = &x->x_rstate.s3;
    double temp, c = 0, f = ((double)(random_frand(s1, s2, s3)));
    if(d){
        f = (f * 0.5 + 0.5);
        switch(d){
            case 1: // CAUCHY: X has a*tan((z-0.5)*pi)
                // first principles of Cauchy re-integrated with a normalisation constant choice of 10
                // such that f = 0.95 gives about 0.35 for temp, could go with 2 to make it finer
                c = atan(10.0 * p);  // PERHAPS CHANGE TO a=1/a;
                // incorrect- missed out divisor of pi in norm temp = a*tan(c*(2*pi*f - 1));
                temp = (1 / p) * tan(c * (2 * f - 1)); // Cauchy distribution, C is precalculated
                return(temp * 0.1); // (temp+100) / 200;
            case 2 : // Logist: X has -(log((1-z)/z)+b)/a which is not very usable as is
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
                return(2.0 * p - 1.0);
            default:
                break;
        }
    }
    return(f); // default, linear/uniform
}

static void gendyn_seed(t_gendyn *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
    uint32_t *s1 = &x->x_rstate.s1, *s2 = &x->x_rstate.s2, *s3 = &x->x_rstate.s3;
    for(int ch = 0; ch < x->x_nchans; ch++){
        x->x_i[ch] = 0;
        x->x_phase[ch] = 1.0;
        for(int i = 0; i < x->x_n; i++){
            x->x_amps[ch * x->x_n + i] = (double)(random_frand(s1, s2, s3));
            x->x_freqs[ch * x->x_n + i] = (double)(random_frand(s1, s2, s3)) * 0.5 + 0.5;
        }
    }
}

static void gendyn_list(t_gendyn *x, t_symbol *s, int ac, t_atom * av){
   (void)s;
    if(ac == 0)
        return;
    if(x->x_list_size != ac){
        x->x_list_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_freq_list[i] = atom_getfloat(av+i);
}

static void gendyn_set(t_gendyn *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(ac != 2)
        return;
    int i = atom_getint(av);
    float f = atom_getint(av+1);
    if(i >= x->x_list_size)
        i = x->x_list_size;
    if(i <= 0)
        i = 1;
    i--;
    x->x_freq_list[i] = f;
}

/*static void gendyn_n(t_gendyn* x, t_floatarg f){ // number of points
    x->x_n = f < 1 ? 1 : f > MAX_N ? MAX_N : (int)f;
    for(int i = 0; i < x->x_nchans; i++)
        x->x_changed[i] = 1;
}*/

static void gendyn_n(t_gendyn* x, t_floatarg f){
    int newn = f < 1 ? 1 : f > MAX_N ? MAX_N : (int)f;
    if(newn != x->x_n){
        x->x_amps = (double *)resizebytes(x->x_amps,
            x->x_nchans * x->x_n * sizeof(double),
            x->x_nchans * newn * sizeof(double));
        x->x_freqs = (double *)resizebytes(x->x_freqs,
            x->x_nchans * x->x_n * sizeof(double),
            x->x_nchans * newn * sizeof(double));
        x->x_n = newn;
        for(int ch = 0; ch < x->x_nchans; ch++)
            x->x_i[ch] = 0; // avoid stale index into resized buffer
        gendyn_seed(x, gensym("seed"), 0, NULL); // refill with valid data
    }
    for(int i = 0; i < x->x_nchans; i++)
        x->x_changed[i] = 1;
}

static void gendyn_bw(t_gendyn* x, t_floatarg f){
    x->x_bw = f < 0 ? 0 : f;
    for(int i = 0; i < x->x_nchans; i++)
        x->x_changed[i] = 1;
}

static void gendyn_cents(t_gendyn* x, t_floatarg f){
    x->x_cents = f != 0;
    for(int i = 0; i < x->x_nchans; i++)
        x->x_changed[i] = 1;
}

static void gendyn_frange(t_gendyn* x, t_floatarg minf, t_floatarg maxf){
    x->x_cents = 0;
    x->x_bw = (maxf - minf) * 0.5;
//    x->x_cf = minf + x->x_bw;
}

static void gendyn_step_a(t_gendyn* x, t_floatarg f){ // maximum amplitude step in percentage
    x->x_astep = (f < 0 ? 0 : f > 100 ? 100 : f) / 50;
}

static void gendyn_step_f(t_gendyn* x, t_floatarg f){ // maximum frequency step in percentage
    x->x_fstep = (f < 0 ? 0 : f > 100 ? 100 : f) / 100;
}

/*static void gendyn_amp_dist(t_gendyn* x, t_floatarg f1, t_floatarg f2){
    x->x_ad = f1 < 0 ? 0 : f1 > 6 ? 6 : (int)f1;
    x->x_ap = f2 < 0.0001 ? 0.0001 : f2 > 1 ? 1 : f2;
}

static void gendyn_freq_dist(t_gendyn* x, t_floatarg f1, t_floatarg f2){
    x->x_fd = f1 < 0 ? 0 : f1 > 6 ? 6 : (int)f1;
    x->x_fp = f2 < 0.0001 ? 0.0001 : f2 > 1 ? 1 : f2;
}*/

static void gendyn_dist(t_gendyn* x, t_floatarg f1, t_floatarg f2, t_floatarg f3, t_floatarg f4){
    x->x_ad = f1 < 0 ? 0 : f1 > 6 ? 6 : (int)f1;
    x->x_ap = f2 < 0.0001 ? 0.0001 : f2 > 1 ? 1 : f2;
    x->x_fd = f3 < 0 ? 0 : f3 > 6 ? 6 : (int)f3;
    x->x_fp = f4 < 0.0001 ? 0.0001 : f4 > 1 ? 1 : f4;
}

static void gendyn_interp(t_gendyn* x, t_floatarg f){
    x->x_interp = f < 0 ? 0 : f > 2 ? 2 : (int)f;
}

static t_int* gendyn_perform(t_int* w){
    t_gendyn *x = (t_gendyn*)(w[1]);
    t_float *in = (t_float*)(w[2]);
    t_float *out = (t_float*)(w[3]);
    
    double *phase = x->x_phase;
    double *amp = x->x_amp; // current index amplitude
    double *freq = x->x_freq; // current index frequency
    t_float *last_cf = x->x_last_cf;
    double *nextamp = x->x_nextamp;
    double *phase_step = x->x_phase_step;
    
    double output = 0.;
    float minf, maxf;
    int n = x->x_nblock, chs = x->x_nchans;
    for(int i = 0; i < n; i++){
        for(int j = 0; j < x->x_nchans; j++){
            float cf; // center frequency
            if(x->x_sig1)
                cf = chs == 1 ? in[i] : in[j*n + i];
            else{
                if(chs == 1)
                    cf = x->x_freq_list[0];
                else
                    cf = x->x_freq_list[j];
            }
            if(phase[j] >= 1){ // get new values
                phase[j] -= 1;
                amp[j] = x->x_amps[j * x->x_n + x->x_i[j]];
                freq[j] = x->x_freqs[j * x->x_n + x->x_i[j]];
                double r = gendyn_rand(x, x->x_ad, x->x_ap) * x->x_astep;
                x->x_amps[j * x->x_n + x->x_i[j]] = gendyn_fold(amp[j] + r, -1.0, 1.0); // update amp
                r = gendyn_rand(x, x->x_fd, x->x_fp) * x->x_fstep;
                x->x_freqs[j * x->x_n + x->x_i[j]] = gendyn_fold(freq[j] + r, 0.0, 1.0); // update freq
                x->x_i[j] = (x->x_i[j] + 1) % x->x_n; // next index
                nextamp[j] = x->x_amps[j * x->x_n + x->x_i[j]]; // next index's amp
                x->x_changed[j] = 1;
            }
            if(cf != x->x_last_cf[j])
                x->x_changed[j] = 1;
            if(x->x_changed[j]){  // need to remap
                if(x->x_cents){
                    float ratio = pow(2, (x->x_bw/1200));
                    minf = cf / ratio;
                    maxf = cf * ratio;
                }
                else{
                    minf = cf - x->x_bw;
                    maxf = cf + x->x_bw;
                }
                if(minf < 0.000001)
                    minf = 0.000001;
                if(minf > 22050)
                    minf = 22050;
                if(maxf < 0.000001)
                    maxf = 0.000001;
                if(maxf > 22050)
                    maxf = 22050;
                float frange = maxf - minf;
                phase_step[j] = freq[j] * frange + minf;
                phase_step[j] *= (x->x_n * x->x_i_sr);
                if(phase_step[j] > 1)
                    phase_step[j] = 1;
                x->x_changed[j] = 0;
            }
            switch(x->x_interp){
                case 0: // no interpolation
                    output = amp[j];
                    break;
                case 1: // linear
                    output = interp_lin(phase[j], amp[j], nextamp[j]);
                    break;
                case 2: // cos
                    output = interp_cos(phase[j], amp[j], nextamp[j]);
                    break;
            }
            phase[j] += phase_step[j];
            out[j*n + i] = output;
            last_cf[j] = cf;
        }
    }
    x->x_phase = phase;
    x->x_amp = amp;
    x->x_freq = freq;
    x->x_nextamp = nextamp;
    x->x_phase_step = phase_step;
    x->x_last_cf = last_cf;
    return(w+4);
}

static void gendyn_dsp(t_gendyn* x, t_signal** sp){
    x->x_i_sr = 1.0 / sys_getsr();
    x->x_nblock = sp[0]->s_n;
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int chs = x->x_sig1 ? sp[0]->s_nchans : x->x_list_size;
    if(x->x_nchans != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_amp = (double *)resizebytes(x->x_amp,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_freq = (double *)resizebytes(x->x_freq,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nextamp = (double *)resizebytes(x->x_nextamp,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase_step = (double *)resizebytes(x->x_phase_step,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_last_cf = (t_float *)resizebytes(x->x_last_cf,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_i = (int *)resizebytes(x->x_i,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_amps = (double *)resizebytes(x->x_amps,
            x->x_nchans * x->x_n * sizeof(double),
            chs * x->x_n * sizeof(double));
        x->x_freqs = (double *)resizebytes(x->x_freqs,
            x->x_nchans * x->x_n * sizeof(double),
            chs * x->x_n * sizeof(double));
        x->x_changed = (int *)resizebytes(x->x_changed,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        for(int i = 0; i < chs; i++)
            x->x_changed[i] = 1;
        x->x_nchans = chs;
        gendyn_seed(x, gensym("seed"), 0, NULL); // rethink
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(gendyn_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void *gendyn_free(t_gendyn *x){
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_amp, x->x_nchans * sizeof(*x->x_amp));
    freebytes(x->x_freq, x->x_nchans * sizeof(*x->x_freq));
    freebytes(x->x_nextamp, x->x_nchans * sizeof(*x->x_nextamp));
    freebytes(x->x_phase_step, x->x_nchans * sizeof(*x->x_phase_step));
    freebytes(x->x_last_cf, x->x_nchans * sizeof(*x->x_last_cf));
    freebytes(x->x_i, x->x_nchans * sizeof(*x->x_i));
    freebytes(x->x_amps, x->x_nchans * x->x_n * sizeof(*x->x_amps));
    freebytes(x->x_freqs, x->x_nchans * x->x_n * sizeof(*x->x_freqs));
    freebytes(x->x_changed, x->x_nchans * sizeof(*x->x_changed));
    free(x->x_freq_list);
    return(void *)x;
}

static void* gendyn_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_gendyn* x = (t_gendyn*)pd_new(gendyn_class);
    int n = 12;                     // number of points
    int ad = 0, fd = 0;             // distribution number
    double ap = 0.5, fp = 0.5;      // distribution parameter
    float fstep = 25, astep = 25;   // steps in %
    x->x_interp = 1;                // interpolation mode
    
    x->x_list_size = 1;
    x->x_nchans = 1;
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_phase[0] = 1;
    x->x_freq = (double *)getbytes(sizeof(*x->x_freq));
    x->x_freq[0] = 0;
    x->x_amp = (double *)getbytes(sizeof(*x->x_amp));
    x->x_amp[0] = 0;
    x->x_nextamp = (double *)getbytes(sizeof(*x->x_nextamp));
    x->x_nextamp[0] = 0;
    x->x_phase_step = (double *)getbytes(sizeof(*x->x_phase_step));
    x->x_phase_step[0] = 0;
    x->x_last_cf = (t_float *)getbytes(sizeof(*x->x_last_cf));
    x->x_last_cf[0] = 0;
    x->x_i = (int *)getbytes(sizeof(*x->x_i));
    x->x_i[0] = 0;
    x->x_changed = (int *)getbytes(sizeof(*x->x_changed));
    x->x_changed[0] = 1;
    
    float cf = 440;
    int seed_flag = 0;
    float seed = 0;
    x->x_bw = 1200;
    x->x_cents = 1;
    x->x_id = random_get_id();
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            s = atom_getsymbol(av);
            if(s == gensym("-interp")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    gendyn_interp(x, atom_getfloatarg(1, ac, av));
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(ac >= 2 && atom_getsymbol(av) == gensym("-seed")){
                seed_flag = 1;
                seed = atom_getfloat(av+1);
                ac-=2, av+=2;
            }
            else if(s == gensym("-fstep")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    fstep = (atom_getfloatarg(1, ac, av));
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(s == gensym("-astep")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    astep = (atom_getfloatarg(1, ac, av));
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(s == gensym("-n")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    n = atom_getintarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(s == gensym("-frange")){
                if(ac >= 3 && (av+1)->a_type == A_FLOAT && (av+2)->a_type == A_FLOAT){
                    float minf = atom_getfloatarg(1, ac, av);
                    float maxf = atom_getfloatarg(2, ac, av);
                    x->x_cents = 0;
                    x->x_bw = (maxf - minf) * 0.5;
                    cf = minf + x->x_bw;
                    ac-=3, av+=3;
                }
                else goto errstate;
            }
            else if(s == gensym("-bw")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_bw = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(s == gensym("-cents")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_cents = atom_getfloatarg(1, ac, av) != 0;
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(atom_getsymbol(av) == gensym("-mc")){
                ac--, av++;
                if(!ac || av->a_type != A_FLOAT)
                    goto errstate;
                int n = 0;
                while(ac && av->a_type == A_FLOAT){
                    x->x_freq_list[n] = atom_getfloat(av);
                    ac--, av++, n++;
                }
                x->x_list_size = n;
            }
            else if(s == gensym("-dist")){
                if(ac >= 5 && (av+1)->a_type == A_FLOAT && (av+2)->a_type == A_FLOAT
                && (av+3)->a_type == A_FLOAT && (av+4)->a_type == A_FLOAT){
                    ad = (int)atom_getfloatarg(1, ac, av);
                    ad = ad < 0 ? 0 : ad > 6 ? 6 : ad;
                    ap = (int)atom_getfloatarg(2, ac, av);
                    ap = ap < 0.0001  ? 0.0001 : ap > 1 ? 1 : ap;
                    fd = (int)atom_getfloatarg(3, ac, av);
                    fd = fd < 0 ? 0 : fd > 6 ? 6 : fd;
                    fp = (int)atom_getfloatarg(4, ac, av);
                    fp = fp < 0.0001 ? 0.0001 : fp > 1 ? 1 : fp;
                    ac-=5, av+=5;
                }
                else goto errstate;
            }
            else goto errstate;
        }
        else goto errstate;
    }
    x->x_freq_list[0] = cf;
    x->x_n = n < 1 ? 1 : n > MAX_N ? MAX_N : n;
    x->x_amps = (double *)getbytes(x->x_n * sizeof(*x->x_amps));
    x->x_freqs = (double *)getbytes(x->x_n * sizeof(*x->x_freqs));
    
    if(seed_flag){
        t_atom at[1];
        SETFLOAT(at, seed);
        ac-=2, av+=2;
        gendyn_seed(x, s, 1, at);
    }
    else
        gendyn_seed(x, s, 0, NULL);
    gendyn_step_a(x, astep);
    gendyn_step_f(x, fstep);
    x->x_ad = ad;
    x->x_fd = fd;
    x->x_ap = ap;
    x->x_fp = fp;
    x->x_i_sr = 1 / sys_getsr();
    x->x_glist = canvas_getcurrent();
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
errstate:
    pd_error(x, "[gendyn~]: improper args");
    return(NULL);
}

void gendyn_tilde_setup(void){
    gendyn_class = class_new(gensym("gendyn~"), (t_newmethod)gendyn_new,
        (t_method)gendyn_free, sizeof(t_gendyn), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addlist(gendyn_class, gendyn_list);
    class_addmethod(gendyn_class, nullfn, gensym("signal"), 0);
    class_addmethod(gendyn_class, (t_method)gendyn_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(gendyn_class, (t_method)gendyn_bw, gensym("bw"), A_FLOAT, 0);
    class_addmethod(gendyn_class, (t_method)gendyn_cents, gensym("cents"), A_FLOAT, 0);
    class_addmethod(gendyn_class, (t_method)gendyn_frange, gensym("frange"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(gendyn_class, (t_method)gendyn_step_a, gensym("amp_step"), A_FLOAT, 0);
    class_addmethod(gendyn_class, (t_method)gendyn_step_f, gensym("freq_step"), A_FLOAT, 0);
    class_addmethod(gendyn_class, (t_method)gendyn_dist, gensym("dist"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(gendyn_class, (t_method)gendyn_n, gensym("n"), A_FLOAT, 0);
    class_addmethod(gendyn_class, (t_method)gendyn_set, gensym("set"), A_GIMME, 0);
//    class_addmethod(gendyn_class, (t_method)gendyn_amp_dist, gensym("amp_dist"), A_FLOAT, 0);
//    class_addmethod(gendyn_class, (t_method)gendyn_freq_dist, gensym("freq_dist"), A_FLOAT, 0);
    class_addmethod(gendyn_class, (t_method)gendyn_interp, gensym("interp"), A_FLOAT, 0);
    class_addmethod(gendyn_class, (t_method)gendyn_seed, gensym("seed"), A_GIMME, 0);
}
