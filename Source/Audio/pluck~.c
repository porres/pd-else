// Porres 2017-2026

#include <m_pd.h>
#include <random.h>
#include <magic.h>
#include <math.h>
#include <stdlib.h>

#define TWO_PI      (3.14159265358979323846 * 2)
#define DEF_RADIANS (0.31830989569 * 0.5)
#define PLUCK_STACK 48000       // stack buf size
#define PLUCK_MAXD  4294967294  // max delay = 2**32 - 2

static t_class *pluck_class;

typedef struct _pluck{
    t_object        x_obj;
    t_glist        *x_glist;
    t_random_state  x_rstate;
    t_inlet        *x_trig_let;
    t_inlet        *x_decay_inlet;
    t_inlet        *x_cutoff_inlet;
    t_int           x_midi_mode;
    float           x_midi_pitch;
    float           x_sr;
    t_int           x_midi;
    t_float         x_freq;
    t_symbol       *x_ignore;
    float           x_float_trig;
    int             x_control_trig;
    int             x_noise_input;
    int             x_sig_in;
//    int             x_debug;
    // pointers to the delay buf
    double         *x_ybuf;
    double         *x_fbstack;
    int             x_alloc;                    // if we are using allocated buf
    unsigned int    x_sz;                       // actual per-channel size of the delay buffer
    unsigned int    x_heapcap;                  // total doubles currently malloc'd (0 if using stack buf)
    unsigned int   *x_wh;                       // writehead
    unsigned int   *x_sum;
    float          *x_amp;
    float          *x_last_trig;
    double         *x_xnm1;
    double         *x_ynm1;
    double         *x_fb;
    double         *x_a0;
    double         *x_a1;
    double         *x_b1;

    // PER-CHANNEL state (previously single scalars shared across all
    // channels -- this was the actual bug causing parameters to update
    // incorrectly / inconsistently in multichannel mode)
    float          *x_hz;      // current freq per channel
    float          *x_ain;     // current decay input per channel
    double         *x_f;       // current cutoff per channel
    double         *x_delms;   // current delay time (ms) per channel
    int            *x_samps;   // current delay time (samples) per channel

    t_int           x_n;
    t_int           x_nchans;
    t_int           x_ch2;
    t_int           x_ch3;
    t_int           x_ch4;
    t_int           x_ch5;
}t_pluck;

void pluck_midi_active(t_pluck *x, t_floatarg f){
    x->x_midi_mode = (int)(f != 0);
}

/*void pluck_debug(t_pluck *x, t_floatarg f){
    x->x_debug = (int)(f != 0);
    post("[pluck~] debug %s", x->x_debug ? "on" : "off");
}*/

static void update_coeffs(t_pluck *x, double f, int j){
    x->x_f[j] = f;
    double omega = f * TWO_PI/x->x_sr;
    if(omega < 0)
        omega = 0;
    if(omega > 2){
        x->x_a0[j] = 1;
        x->x_a1[j] = x->x_b1[j] = 0;
    }
    else{
        x->x_a0[j] = x->x_a1[j] = omega * 0.5;
        x->x_b1[j] = 1 - omega;
    }
}

static void update_fb(t_pluck *x, double fb, double delms, int j){
    x->x_ain[j] = (float)fb;
    x->x_fb[j] = fb == 0 ? 0 : copysign(exp(log(0.001) * delms/fabs(fb)), fb);
}

static void update_time(t_pluck *x, float hz, int j){
    double period = 1./(double)hz;
    x->x_delms[j] = period * 1000;
    x->x_samps[j] = (int)roundf(period * x->x_sr);
}

static void pluck_clear(t_pluck *x){
    for(unsigned int i = 0; i < x->x_sz * x->x_nchans; i++)
        x->x_ybuf[i] = 0.;
    for(int j = 0; j < x->x_nchans; j++){
        x->x_xnm1[j] = x->x_ynm1[j] = 0.;
        x->x_wh[j] = 0;
    }
}

static void pluck_midi(t_pluck *x, t_floatarg f){
    x->x_midi = (int)(f != 0);
}

static void pluck_list(t_pluck *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(atom_getfloat(av+1) == 0)
        return;
    if(ac >= 2){
        x->x_midi_pitch = atom_getfloat(av);
        ac--, av++;
        x->x_float_trig = atom_getfloat(av) / 127.f;
        x->x_control_trig = 1;
    }
}

// Ensures x_ybuf has room for x_sz * x_nchans doubles, using the stack
// buffer (x_fbstack) whenever possible and falling back to a heap
// allocation otherwise. Must be called any time x_sr OR x_nchans changes.
// NOTE: x_fbstack itself must already be sized to PLUCK_STACK * x_nchans
// by the caller (pluck_dsp keeps it in sync whenever nchans changes)
// before this runs.
static void pluck_sz(t_pluck *x){
    unsigned int newsz = (unsigned int)x->x_sr + 1;
    if(newsz > PLUCK_MAXD)
        newsz = PLUCK_MAXD;
    unsigned int needed = newsz * (unsigned int)x->x_nchans;
    if(newsz > PLUCK_STACK){ // doesn't fit in the stack buffer -> need heap
        if(!x->x_alloc){
            x->x_ybuf = (double *)malloc(sizeof(double) * needed);
            x->x_alloc = 1;
        }
        else if(needed != x->x_heapcap){
            x->x_ybuf = (double *)realloc(x->x_ybuf, sizeof(double) * needed);
        }
        x->x_heapcap = needed;
        x->x_sz = newsz;
    }
    else{ // fits in the stack buffer
        if(x->x_alloc){
            free(x->x_ybuf);
            x->x_alloc = 0;
            x->x_heapcap = 0;
        }
        x->x_ybuf = x->x_fbstack; // already sized to PLUCK_STACK * x_nchans
        x->x_sz = PLUCK_STACK;
    }
    pluck_clear(x);
}

static double pluck_getlin(double tab[], unsigned int sz, double idx){
    // linear interpolated reader, copied from Derek Kwan's library
    double output;
    unsigned int tabphase1 = (unsigned int)idx;
    unsigned int tabphase2 = tabphase1 + 1;
    double frac = idx - (double)tabphase1;
    if(tabphase1 >= sz - 1){
        tabphase1 = sz - 1; // checking to see if index falls within bounds
        output = tab[tabphase1];
    }
    else{
        double yb = tab[tabphase2]; // linear interp
        double ya = tab[tabphase1];
        output = ya+((yb-ya)*frac);
    };
    return(output);
}

static double pluck_read_delay(t_pluck *x, double arr[], int in_samps, int j){
    double rh = in_samps < 1 ? 1 : (double)in_samps; // read head size in samples
    rh = (double)x->x_wh[j] + ((double)x->x_sz-rh);
    while(rh >= x->x_sz) // wrap to delay buffer length
        rh -= (double)x->x_sz;
    return(pluck_getlin(arr, x->x_sz, rh)); // read from buffer
}

////////////////////////////////////////////////////////////////////////////////////

static t_int *pluck_perform_noise_input(t_int *w){
    t_pluck *x = (t_pluck *)(w[1]);
    t_float *hz_in = (t_float *)(w[2]);
    t_float *t_in = (t_float *)(w[3]);
    t_float *ain = (t_float *)(w[4]);
    t_float *cut_in = (t_float *)(w[5]);
    t_float *noise_in = (t_float *)(w[6]);
    t_float *out = (t_float *)(w[7]);
    t_float sr = x->x_sr;
    t_float nyq = sr * 0.5;
    t_float *last_trig = x->x_last_trig;
    unsigned int *sum = x->x_sum;
    t_float *amp = x->x_amp;
    double *xnm1 = x->x_xnm1;
    double *ynm1 = x->x_ynm1;
    int n = x->x_n, chs2 = x->x_ch2, chs3 = x->x_ch3, chs4 = x->x_ch4;
    int chs = x->x_nchans, chs5 = x->x_ch5;
    for(int j = 0; j < chs; j++){
        for(int i = 0; i < n; i++){
            t_float hz =  chs == 1 ? hz_in[i] : hz_in[j*n + i];
            t_float trig = chs2 == 1 ? t_in[i] : t_in[j*n + i];
            float a_in = chs3 == 1 ? ain[i] : ain[j*n + i];
            if(x->x_midi_mode || !x->x_sig_in){
                hz = x->x_midi_pitch;
                trig = 0;
            }
            if(hz < 1){
                out[j*n + i] = sum[j] = 0;
                xnm1[j] = ynm1[j] = 0;
            }
            else{
                if(x->x_midi){
                    if(hz > 127)
                        hz = 127;
                    hz = hz <= 0 ? 0 : pow(2, (hz - 69)/12) * 440;
                }
                if(hz != x->x_hz[j]){
                    x->x_hz[j] = hz;
                    update_time(x, hz, j);
                    goto update_fb;
                }
                if(x->x_ain[j] != a_in){
                    update_fb:
                    update_fb(x, a_in, x->x_delms[j], j);
                }
                // get delayed vals
                double fb_del = pluck_read_delay(x, x->x_ybuf + j * x->x_sz, x->x_samps[j], j);
                if((trig != 0 && last_trig[j] == 0) || x->x_control_trig){ // trigger
                    amp[j] = x->x_control_trig ? x->x_float_trig : trig;
                    sum[j] = 0;
                    x->x_control_trig = 0;
                }
    // Filter stuff
                double cuttoff = chs4 == 1 ? cut_in[i] : cut_in[j*n + i];
                if(cuttoff < 0.000001)
                    cuttoff = 0.000001;
                if(cuttoff > nyq - 0.000001)
                    cuttoff = nyq - 0.000001;
                if(x->x_f[j] != cuttoff)
                    update_coeffs(x, cuttoff, j);
                // gate
                t_float gate = (sum[j]++ <= x->x_samps[j]) * amp[j];
                // noise
                t_float noise_input = chs5 == 1 ? noise_in[i] : noise_in[j*n + i];
                t_float noise = gate ? noise_input * gate : 0;
                // output
                double output = (double)noise + x->x_fb[j] * fb_del;
                out[j*n + i] = output;

                double yn = x->x_a0[j] * output + x->x_a1[j] * xnm1[j] + x->x_b1[j] * ynm1[j];

                // put into delay buffer
                int wh = x->x_wh[j];
                x->x_ybuf[j * x->x_sz + wh] = yn;
                x->x_wh[j] = (wh + 1) % x->x_sz; // increment writehead

/*                if(x->x_debug && i == 0){
                    post("[pluck~ ch %d] hz=%.3f trig=%.3f decay=%.3f cutoff=%.1f samps=%d fb=%.5f",
                        j, hz, trig, a_in, cuttoff, x->x_samps[j], x->x_fb[j]);
                }*/

                last_trig[j] = trig;
                xnm1[j] = output;
                ynm1[j] = yn;
            }
        }
    }
    x->x_sum = sum; // next
    x->x_amp = amp;
    x->x_last_trig = last_trig;
    x->x_xnm1 = xnm1;
    x->x_ynm1 = ynm1;
    return(w+8);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

static t_int *pluck_perform(t_int *w){
    t_pluck *x = (t_pluck *)(w[1]);
    t_random_state *rstate = (t_random_state *)(w[2]);
    t_float *hz_in = (t_float *)(w[3]);
    t_float *t_in = (t_float *)(w[4]);
    t_float *ain = (t_float *)(w[5]);
    t_float *cut_in = (t_float *)(w[6]);
    t_float *out = (t_float *)(w[7]);
    uint32_t *s1 = &rstate->s1;
    uint32_t *s2 = &rstate->s2;
    uint32_t *s3 = &rstate->s3;
    t_float nyq = x->x_sr * 0.5;
    t_float *last_trig = x->x_last_trig;
    unsigned int *sum = x->x_sum;
    t_float *amp = x->x_amp;
    double *xnm1 = x->x_xnm1;
    double *ynm1 = x->x_ynm1;
    int n = x->x_n, chs2 = x->x_ch2, chs3 = x->x_ch3, chs4 = x->x_ch4;
    int chs = x->x_nchans;
    for(int j = 0; j < chs; j++){
        for(int i = 0; i < n; i++){
            t_float hz =  chs == 1 ? hz_in[i] : hz_in[j*n + i];
            t_float trig = chs2 == 1 ? t_in[i] : t_in[j*n + i];
            float a_in = chs3 == 1 ? ain[i] : ain[j*n + i];
            if(x->x_midi_mode || !x->x_sig_in){
                hz = x->x_midi_pitch;
                trig = 0;
            }
            if(hz < 1){
                out[j*n + i] = sum[j] = 0;
                xnm1[j] = ynm1[j] = 0;
            }
            else{
                if(x->x_midi && hz < 256)
                    hz = pow(2, (hz - 69)/12) * 440;
                if(hz != x->x_hz[j]){
                    x->x_hz[j] = hz;
                    update_time(x, hz, j);
                    goto update_fb;
                }
                if(x->x_ain[j] != a_in){
                    update_fb:
                    update_fb(x, a_in, x->x_delms[j], j);
                }
                // get delayed vals
                double fb_del = pluck_read_delay(x, x->x_ybuf + j * x->x_sz, x->x_samps[j], j);
                if((trig != 0 && last_trig[j] == 0) || x->x_control_trig){ // trigger
                    amp[j] = x->x_control_trig ? x->x_float_trig : trig;
                    sum[j] = 0;
                    x->x_control_trig = 0;
                }
                // Filter stuff
                double cuttoff = chs4 == 1 ? cut_in[i] : cut_in[j*n + i];
                if(cuttoff < 0.000001)
                    cuttoff = 0.000001;
                if(cuttoff > nyq - 0.000001)
                    cuttoff = nyq - 0.000001;
                if(x->x_f[j] != cuttoff)
                    update_coeffs(x, cuttoff, j);
                // gate
                t_float gate = (sum[j]++ <= x->x_samps[j]) * amp[j];
                // noise
                t_float noise = (gate != 0) ? (t_float)(random_frand(s1, s2, s3)) * gate : 0;
                // output
                double output = (double)noise + x->x_fb[j] * fb_del;
                out[j*n + i] = output;

                double yn = x->x_a0[j] * output + x->x_a1[j] * xnm1[j] + x->x_b1[j] * ynm1[j];

                // put into delay buffer
                int wh = x->x_wh[j];
                x->x_ybuf[j * x->x_sz + wh] = yn;
                x->x_wh[j] = (wh + 1) % x->x_sz; // increment writehead

/*                if(x->x_debug && i == 0){
                    post("[pluck~ ch %d] hz=%.3f trig=%.3f decay=%.3f cutoff=%.1f samps=%d fb=%.5f",
                        j, hz, trig, a_in, cuttoff, x->x_samps[j], x->x_fb[j]);
                }*/

                last_trig[j] = trig;

                xnm1[j] = output;
                ynm1[j] = yn;
            }
        }
    }
    x->x_sum = sum; // next
    x->x_amp = amp;
    x->x_last_trig = last_trig;
    x->x_xnm1 = xnm1;
    x->x_ynm1 = ynm1;
    return(w+8);
}

static void pluck_dsp(t_pluck *x, t_signal **sp){
    int sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    x->x_sig_in = sig1 || sig2;
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans, x->x_ch3 = sp[2]->s_nchans, x->x_ch4 = sp[3]->s_nchans;
    if(x->x_noise_input)
        x->x_ch5 = sp[4]->s_nchans;
//    if(x->x_debug)
//        post("[pluck~] dsp: nchans %d -> %d, sig_in=%d", x->x_nchans, chs, x->x_sig_in);
    if(x->x_nchans != chs){
        x->x_sum = (unsigned int *)resizebytes(x->x_sum,
            x->x_nchans * sizeof(unsigned int), chs * sizeof(unsigned int));
        x->x_amp = (float *)resizebytes(x->x_amp,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_last_trig = (float *)resizebytes(x->x_last_trig,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_xnm1 = (double *)resizebytes(x->x_xnm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_fb = (double *)resizebytes(x->x_fb,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_a0 = (double *)resizebytes(x->x_a0,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_a1 = (double *)resizebytes(x->x_a1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_b1 = (double *)resizebytes(x->x_b1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_wh = (unsigned int *)resizebytes(x->x_wh,
            x->x_nchans * sizeof(unsigned int), chs * sizeof(unsigned int));
        // per-channel parameter state (the fix)
        x->x_hz = (float *)resizebytes(x->x_hz,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_ain = (float *)resizebytes(x->x_ain,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_f = (double *)resizebytes(x->x_f,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_delms = (double *)resizebytes(x->x_delms,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_samps = (int *)resizebytes(x->x_samps,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        // keep the stack delay buffer sized to PLUCK_STACK * nchans so it's
        // always valid to fall back to, regardless of current sr-driven alloc state
        x->x_fbstack = (double *)resizebytes(x->x_fbstack,
            x->x_nchans * PLUCK_STACK * sizeof(*x->x_fbstack),
            chs * PLUCK_STACK * sizeof(*x->x_fbstack));
        if(!x->x_alloc)
            x->x_ybuf = x->x_fbstack; // resizebytes may have moved it
        x->x_nchans = chs;
        pluck_sz(x);
    }
    int sr = sp[0]->s_sr;
    if(sr != x->x_sr){ // if new sample rate isn't old sample rate, need to realloc
        x->x_sr = sr;
        pluck_sz(x);
        for(int j = 0; j < x->x_nchans; j++)
            update_coeffs(x, x->x_f[j], j);
    };
    if(x->x_noise_input){
        signal_setmultiout(&sp[5], x->x_nchans);
        if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
        || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)
        || (x->x_ch4 > 1 && x->x_ch4 != x->x_nchans)
        || (x->x_ch5 > 1 && x->x_ch5 != x->x_nchans)){
            dsp_add_zero(sp[5]->s_vec, x->x_nchans*x->x_n);
            pd_error(x, "[pluck~]: channel sizes mismatch");
            return;
        }
        dsp_add(pluck_perform_noise_input, 7, x, sp[0]->s_vec,
                sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
    }
    else{
        signal_setmultiout(&sp[4], x->x_nchans);
        if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
        || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)
        || (x->x_ch4 > 1 && x->x_ch4 != x->x_nchans)){
            dsp_add_zero(sp[4]->s_vec, x->x_nchans*x->x_n);
            pd_error(x, "[pluck~]: channel sizes mismatch");
            return;
        }
        dsp_add(pluck_perform, 7, x, &x->x_rstate, sp[0]->s_vec,
                sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
    }
}

static void * pluck_free(t_pluck *x){
    if(x->x_alloc)
        free(x->x_ybuf);
    freebytes(x->x_sum, x->x_nchans * sizeof(*x->x_sum));
    freebytes(x->x_amp, x->x_nchans * sizeof(*x->x_amp));
    freebytes(x->x_last_trig, x->x_nchans * sizeof(*x->x_last_trig));
    freebytes(x->x_xnm1, x->x_nchans * sizeof(*x->x_xnm1));
    freebytes(x->x_ynm1, x->x_nchans * sizeof(*x->x_ynm1));
    freebytes(x->x_fb, x->x_nchans * sizeof(*x->x_fb));
    freebytes(x->x_a0, x->x_nchans * sizeof(*x->x_a0));
    freebytes(x->x_a1, x->x_nchans * sizeof(*x->x_a1));
    freebytes(x->x_b1, x->x_nchans * sizeof(*x->x_b1));
    freebytes(x->x_wh, x->x_nchans * sizeof(*x->x_wh));
    freebytes(x->x_hz, x->x_nchans * sizeof(*x->x_hz));
    freebytes(x->x_ain, x->x_nchans * sizeof(*x->x_ain));
    freebytes(x->x_f, x->x_nchans * sizeof(*x->x_f));
    freebytes(x->x_delms, x->x_nchans * sizeof(*x->x_delms));
    freebytes(x->x_samps, x->x_nchans * sizeof(*x->x_samps));
    freebytes(x->x_fbstack, PLUCK_STACK * x->x_nchans * sizeof(*x->x_fbstack));
    inlet_free(x->x_trig_let);
    inlet_free(x->x_decay_inlet);
    inlet_free(x->x_cutoff_inlet);
    return(void *)x;
}

static void *pluck_new(t_symbol *s, int ac, t_atom *av){
    t_pluck *x = (t_pluck *)pd_new(pluck_class);
    x->x_ignore = s;
    x->x_glist = canvas_getcurrent();
    x->x_sr = sys_getsr();
    static int seed = 1;
    random_init(&x->x_rstate, seed++);
    float freq = 0;
    float decay = 0;
    float cut_freq = DEF_RADIANS * x->x_sr;
    x->x_midi_pitch = 0;
    x->x_midi_mode = 0;
    x->x_midi = 0;
    x->x_float_trig = 1;
    x->x_control_trig = 0;
    x->x_noise_input = 0;
//    x->x_debug = 0;

    x->x_nchans = 1;
    x->x_sum = (unsigned int *)getbytes(sizeof(*x->x_sum));
    x->x_sum[0] = PLUCK_MAXD; // ???
    x->x_amp = (float *)getbytes(sizeof(*x->x_amp));
    x->x_amp[0] = 0.;
    x->x_last_trig = (float *)getbytes(sizeof(*x->x_last_trig));
    x->x_last_trig[0] = 0.;
    x->x_xnm1 = (double *)getbytes(sizeof(*x->x_xnm1));
    x->x_ynm1 = (double *)getbytes(sizeof(*x->x_ynm1));
    x->x_xnm1[0] = x->x_ynm1[0] = 0.;
    x->x_fb = (double *)getbytes(sizeof(*x->x_fb));
    x->x_fb[0] = 0.;
    x->x_a0 = (double *)getbytes(sizeof(*x->x_a0));
    x->x_a1 = (double *)getbytes(sizeof(*x->x_a1));
    x->x_b1 = (double *)getbytes(sizeof(*x->x_b1));
    x->x_a0[0] = x->x_a1[0] = x->x_b1[0] = 0.;
    // per-channel parameter state, nchans == 1 for now
    x->x_hz = (float *)getbytes(sizeof(*x->x_hz));
    x->x_ain = (float *)getbytes(sizeof(*x->x_ain));
    x->x_f = (double *)getbytes(sizeof(*x->x_f));
    x->x_delms = (double *)getbytes(sizeof(*x->x_delms));
    x->x_samps = (int *)getbytes(sizeof(*x->x_samps));
    x->x_hz[0] = 0;
    x->x_ain[0] = 0;
    x->x_f[0] = 0;
    x->x_delms[0] = 0;
    x->x_samps[0] = 0;

    int argnum = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){ //if current argument is a float
            t_float aval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    freq = aval;
                    break;
                case 1:
                    decay = aval;
                    break;
                case 2:
                    cut_freq = aval < 0 ? 0 : aval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--, av++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            t_symbol *curarg = atom_getsymbol(av);
            if(curarg == gensym("-in")){
                x->x_noise_input = 1;
                ac--, av++;
            }
            else if(curarg == gensym("-midi")){
                x->x_midi = 1;
                ac--, av++;
            }
            else if(curarg == gensym("-midi_active")){
                x->x_midi_mode = 1;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
    x->x_alloc = 0;
    x->x_heapcap = 0;
    x->x_sz = PLUCK_STACK;
// clear out stack buf, set pointer to stack
    x->x_fbstack = (double *)getbytes(PLUCK_STACK * x->x_nchans * sizeof(*x->x_fbstack));
    x->x_ybuf = x->x_fbstack;
    x->x_wh = (unsigned int *)getbytes(sizeof(*x->x_wh));
    x->x_wh[0] = 0;
    pluck_clear(x);
    x->x_hz[0] = x->x_freq = (double)freq;

    if(x->x_midi && x->x_hz[0] < 256)
        x->x_hz[0]  = pow(2, (x->x_hz[0]  - 69)/12) * 440;

    x->x_ain[0] = decay;
    x->x_f[0] = (double)cut_freq;
// ship off to the helper method to deal with allocation if necessary
    pluck_sz(x);
    if(x->x_hz[0] >= 1){
        update_time(x, x->x_hz[0], 0);
        update_fb(x, x->x_ain[0], x->x_delms[0], 0);
    }
    if(x->x_f[0] >= 0)
        update_coeffs(x, x->x_f[0], 0);
// inlets / outlet
    x->x_trig_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_decay_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_decay_inlet, decay);
    x->x_cutoff_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_cutoff_inlet, cut_freq);
    if(x->x_noise_input)
        inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "[pluck~]: improper args");
    return(NULL);
}

void pluck_tilde_setup(void){
    pluck_class = class_new(gensym("pluck~"), (t_newmethod)pluck_new,
        (t_method)pluck_free, sizeof(t_pluck), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(pluck_class, (t_method)pluck_dsp, gensym("dsp"), A_CANT, 0);
    CLASS_MAINSIGNALIN(pluck_class, t_pluck, x_freq);
    class_addlist(pluck_class, pluck_list);
    class_addmethod(pluck_class, (t_method)pluck_midi, gensym("midi"), A_DEFFLOAT, 0);
    class_addmethod(pluck_class, (t_method)pluck_midi_active, gensym("midi_active"), A_FLOAT, 0);
    class_addmethod(pluck_class, (t_method)pluck_clear, gensym("clear"), 0);
//    class_addmethod(pluck_class, (t_method)pluck_debug, gensym("debug"), A_DEFFLOAT, 0);
}
