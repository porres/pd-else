// Porres 2017-2023

#include "m_pd.h"
#include "magic.h"
#include "buffer.h"

typedef struct _fbsine{
    t_object    x_obj;
    double     *x_phase;
    float      *x_yn_m1;
    float      *x_yn_m2;
    int         x_nchans;
    t_float     x_freq;
    t_int       x_filter;
    t_inlet    *x_inlet_fb;
    t_inlet    *x_inlet_phase;
    t_inlet    *x_inlet_sync;
    t_outlet   *x_outlet;
    double      x_sr_rec;
// MAGIC:
    t_glist    *x_glist; // object list
    t_float    *x_signalscalar; // right inlet's float field
    int         x_hasfeeders; // right inlet connection flag
    t_float     x_phase_sync_float; // float from magic
}t_fbsine;

static t_class *fbsine_class;

static void fbsine_filter(t_fbsine *x, t_floatarg f){
    x->x_filter = f != 0;
}

double fbsine_wrap_phase(double phase){
    while(phase >= 1)
        phase -= 1.;
    while(phase < 0)
        phase += 1.;
    return(phase);
}

static t_int *fbsine_perform(t_int *w){
    t_fbsine *x = (t_fbsine *)(w[1]);
    int n = (int)(w[2]);
    int ch2 = (int)(w[3]);
    int ch4 = (int)(w[4]);
    t_float *in1 = (t_float *)(w[5]);
    t_float *in2 = (t_float *)(w[6]);
    t_float *in4 = (t_float *)(w[7]);
    t_float *out = (t_float *)(w[8]);
// Magic Start
    t_float *scalar = x->x_signalscalar;
    if(!else_magic_isnan(*x->x_signalscalar)){
        t_float input_phase = fmod(*scalar, 1);
        if(input_phase < 0)
            input_phase += 1;
        for(int j = 0; j < x->x_nchans; j++)
            x->x_phase[j] = input_phase;
        else_magic_setnan(x->x_signalscalar);
    }
// Magic End
    float *yn_m1 = x->x_yn_m1;
    float *yn_m2 = x->x_yn_m2;
    double *phase = x->x_phase;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double hz = in1[j*n + i];
            double phase_step = hz * x->x_sr_rec; // phase_step
            t_float index = ch2 == 1 ? in2[i] : in2[j*n + i];
            t_float phase_offset = ch4 == 1 ? in4[i] : in4[j*n + i];
            float fback;
            if(x->x_filter)
                fback = ((yn_m1[j] + yn_m2[j]) * 0.5) * index;
            else
                fback = yn_m1[j] * index;
            float output = read_sintab(fbsine_wrap_phase(phase[j] + fback + phase_offset));
            out[j*n + i] = output;
            phase[j] = fbsine_wrap_phase(phase[j] + phase_step);
            yn_m2[j] = yn_m1[j];
            yn_m1[j] = output;
        }
    }
    x->x_yn_m1 = yn_m1;
    x->x_yn_m2 = yn_m2;
    x->x_phase = phase;
    return(w+9);
}

static t_int *fbsine_perform_sig(t_int *w){
    t_fbsine *x = (t_fbsine *)(w[1]);
    int n = (t_int)(w[2]);
    int ch2 = (int)(w[3]);
    int ch3 = (int)(w[4]);
    int ch4 = (int)(w[5]);
    t_float *in1 = (t_float *)(w[6]); // freq
    t_float *in2 = (t_float *)(w[7]); // fb
    t_float *in3 = (t_float *)(w[8]); // sync
    t_float *in4 = (t_float *)(w[9]); // phase
    t_float *out = (t_float *)(w[10]);
    double *phase = x->x_phase;
    float *yn_m1 = x->x_yn_m1;
    float *yn_m2 = x->x_yn_m2;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double hz = in1[j*n + i];
            double phase_step = hz * x->x_sr_rec; // phase_step
            t_float index = ch2 == 1 ? in2[i] : in2[j*n + i];
            t_float trig = ch3 == 1 ? in3[i] : in3[j*n + i];
            t_float phase_offset = ch4 == 1 ? in4[i] : in4[j*n + i];
            float fback;
            if(x->x_filter)
                fback = ((yn_m1[j] + yn_m2[j]) * 0.5) * index;
            else
                fback = yn_m1[j] * index;
            double ph = phase[j] + fback + phase_offset;
            if(trig > 0 && trig <= 1)
                ph = phase[j] = (double)trig;
            float output = read_sintab(fbsine_wrap_phase(ph));
            out[j*n + i] = output;
            phase[j] = fbsine_wrap_phase(phase[j]+ phase_step);
            yn_m2[j] = yn_m1[j];
            yn_m1[j] = output;
        }
    }
    x->x_phase = phase;
    x->x_yn_m1 = yn_m1;
    x->x_yn_m2 = yn_m2;
    return(w+11);
}

static void fbsine_dsp(t_fbsine *x, t_signal **sp){
    x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans, ch4 = sp[3]->s_nchans, n = sp[0]->s_n;
    signal_setmultiout(&sp[4], chs);
    if(x->x_nchans != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_yn_m1 = (float *)resizebytes(x->x_yn_m1,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_yn_m2 = (float *)resizebytes(x->x_yn_m2,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_nchans = chs;
    }
    if((ch2 > 1 && ch2 != x->x_nchans) || (ch3 > 1 && ch3 != x->x_nchans) || (ch4 > 1 && ch4 != x->x_nchans)){
        dsp_add_zero(sp[4]->s_vec, chs*n);
        pd_error(x, "[fbsine~]: channel sizes mismatch");
        return;
    }
    x->x_hasfeeders = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal); // magic feeder flag
    if(x->x_hasfeeders)
        dsp_add(fbsine_perform_sig, 10, x, n, ch2, ch3, ch4, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
    else
        dsp_add(fbsine_perform, 8, x, n, ch2, ch4, sp[0]->s_vec, sp[1]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void *fbsine_free(t_fbsine *x){
    inlet_free(x->x_inlet_fb);
    inlet_free(x->x_inlet_sync);
    inlet_free(x->x_inlet_phase);
    outlet_free(x->x_outlet);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_yn_m1, x->x_nchans * sizeof(*x->x_yn_m1));
    freebytes(x->x_yn_m2, x->x_nchans * sizeof(*x->x_yn_m2));
    return(void *)x;
}

static void *fbsine_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_fbsine *x = (t_fbsine *)pd_new(fbsine_class);
    t_float f1 = 0, f2 = 0, f3 = 0, f4 = 1;
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_yn_m1 = (float *)getbytes(sizeof(*x->x_yn_m1));
    x->x_yn_m2 = (float *)getbytes(sizeof(*x->x_yn_m2));
    if(ac && av->a_type == A_FLOAT){
        f1 = av->a_w.w_float;
        ac--; av++;
        if(ac && av->a_type == A_FLOAT){
            f2 = av->a_w.w_float;
            ac--; av++;
            if(ac && av->a_type == A_FLOAT){
                f3 = av->a_w.w_float;
                ac--; av++;
                if(ac && av->a_type == A_FLOAT){
                    f4 = av->a_w.w_float;
                    ac--; av++;
                }
            }
        }
    }
    init_sine_table();
    t_float init_freq = f1;
    t_float init_fb = f2;
    t_float init_phase = f3;
    x->x_filter = f4 != 0;
    init_phase = init_phase < 0 ? 0 : init_phase >= 1 ? 0 : init_phase; // clipping phaseinput
    x->x_freq = init_freq;
    x->x_inlet_fb = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_fb, init_fb);
    x->x_inlet_phase = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_phase, init_phase);
    x->x_inlet_sync = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_sync, 0);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    // Magic
    x->x_glist = canvas_getcurrent();
    x->x_signalscalar = obj_findsignalscalar((t_object *)x, 2);
    return(x);
}

void fbsine_tilde_setup(void){
    fbsine_class = class_new(gensym("fbsine~"), (t_newmethod)fbsine_new, (t_method)fbsine_free, sizeof(t_fbsine), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(fbsine_class, t_fbsine, x_freq);
    class_addmethod(fbsine_class, (t_method)fbsine_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(fbsine_class, (t_method)fbsine_filter, gensym("filter"), A_FLOAT, 0);
}
