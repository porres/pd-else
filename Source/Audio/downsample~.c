// Porres 2017

#include <m_pd.h>
#include "math.h"

static t_class *downsample_class;

typedef struct _downsample{
    t_object x_obj;
    double  *x_phase;
    t_float *x_yn;
    t_float *x_ynm1;
    t_float  x_interp;
    t_inlet *x_inlet;
    float    x_sr;
    int      x_bang;
    t_int    x_n;
    t_int    x_ch1;
    t_int    x_ch2;
    t_int    x_nchs;
    t_symbol *x_ignore;
}t_downsample;

static void downsample_interp(t_downsample *x, t_floatarg f){
    x->x_interp = f != 0;
}

static void downsample_bang(t_downsample *x){
    x->x_bang = 1;
}

static t_int *downsample_perform(t_int *w){
    t_downsample *x = (t_downsample *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    double *phase = x->x_phase;
    t_float *yn = x->x_yn;
    t_float *ynm1 = x->x_ynm1;
    t_float interp = x->x_interp;
    double sr = x->x_sr;
    for(int j = 0; j < x->x_nchs; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            float input;
            if(x->x_ch1 == 1)
                input = in1[i];
            else
                input = in1[j*n + i];
            float hz;
            if(x->x_ch2 == 1)
                hz = in2[i];
            else
                hz = in2[j*n + i];
            double phase_step = hz / sr;
    // clipped phase_step
            phase_step = phase_step > 1 ? 1. : phase_step < -1 ? -1 : phase_step;
            if(hz >= 0){
                if(x->x_bang){
                    phase[j] = 0;
                    x->x_bang = 0;
                    ynm1[j] = yn[j];
                    yn[j] = input;
                }
                else if(phase[j] >= 1.){ // update
                    phase[j]--;
                    ynm1[j] = yn[j];
                    yn[j] = input;
                }
            }
            else{
                if(x->x_bang){
                    phase[j] = 0;
                    x->x_bang = 0;
                    ynm1[j] = yn[j];
                    yn[j] = input;
                }
                else if(phase[j] <= 0.){
                    phase[j]++;
                    ynm1[j] = yn[j];
                    yn[j] = input;
                    ynm1[j] = yn[j];
                    yn[j] = input;
                }
            }
            if(interp){
                if(hz >= 0)
                    out[j*n + i] = ynm1[j] + (yn[j] - ynm1[j]) * (phase[j]);
                else
                    out[j*n + i] = ynm1[j] + (yn[j] - ynm1[j]) * (1 - phase[j]);
            }
            else
                out[j*n + i] = yn[j];
            phase[j] += phase_step;
        }
    }
    x->x_phase = phase;
    x->x_yn = yn;
    x->x_ynm1 = ynm1;
    return(w+5);
}

static void downsample_dsp(t_downsample *x, t_signal **sp){
    x->x_sr = sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    int chs = x->x_ch1 = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    if(x->x_ch2 > chs)
        chs = x->x_ch2;
    if(x->x_nchs != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_yn = (t_float *)resizebytes(x->x_yn,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_ynm1 = (t_float *)resizebytes(x->x_ynm1,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchs = chs;
    }
    signal_setmultiout(&sp[2], x->x_nchs);
    if(x->x_ch1 > 1 && x->x_ch1 != x->x_nchs ||
    x->x_ch2 > 1 && x->x_ch2 != x->x_nchs){
        dsp_add_zero(sp[2]->s_vec, x->x_nchs*x->x_n);
        pd_error(x, "[downsample~]: channel sizes mismatch");
        return;
    }
    dsp_add(downsample_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *downsample_free(t_downsample *x){
    inlet_free(x->x_inlet);
    freebytes(x->x_phase, x->x_nchs * sizeof(*x->x_phase));
    freebytes(x->x_yn, x->x_nchs * sizeof(*x->x_yn));
    freebytes(x->x_ynm1, x->x_nchs * sizeof(*x->x_ynm1));
    return(void *)x;
}

static void *downsample_new(t_symbol *s, int argc, t_atom *argv){
    t_downsample *x = (t_downsample *)pd_new(downsample_class);
    x->x_ignore = s;
    t_float init_freq = sys_getsr();
    x->x_interp = 0;
    x->x_bang = 0;
    x->x_phase = (double *)getbytes(sizeof(double));
    x->x_phase[0] = 0;
    x->x_yn = (t_float *)getbytes(sizeof(t_float));
    x->x_ynm1 = (t_float *)getbytes(sizeof(t_float));
/////////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(argc > 0){
        if(argv->a_type == A_FLOAT){ //if current argument is a float
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum){
                case 0:
                    init_freq = argval;
                    break;
                case 1:
                    x->x_interp = (argval != 0);
                    break;
                default:
                    break;
            };
            argnum++;
            argc--;
            argv++;
        }
        else
            goto errstate;
    }
/////////////////////////////////////////////////////////////////////////////////////
    if(init_freq >= 0)
        x->x_phase[0] = 1;
    x->x_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet, init_freq);
    outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "[downsample~]: improper args");
    return(NULL);
}

void downsample_tilde_setup(void){
    downsample_class = class_new(gensym("downsample~"), (t_newmethod)(void *)downsample_new,
        (t_method)downsample_free, sizeof(t_downsample), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addbang(downsample_class, downsample_bang);
    class_addmethod(downsample_class, nullfn, gensym("signal"), 0);
    class_addmethod(downsample_class, (t_method)downsample_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(downsample_class, (t_method)downsample_interp, gensym("interp"), A_DEFFLOAT, 0);
}
