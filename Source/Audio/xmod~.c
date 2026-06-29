// Porres 2017-2026

#include <m_pd.h>
#include <buffer.h>

static t_class *xmod_class;

typedef struct _xmod{
    t_object    x_obj;
    double     *x_phase1;
    double     *x_phase2;
    t_float    *x_yn;
    t_float     x_pm;
    int         x_n;
    int         x_nchans;
    int         x_ch2;
    int         x_ch3;
    int         x_ch4;
    t_float     x_freq;
    t_inlet    *x_inlet_fb;
    t_inlet    *x_inlet_freq2;
    t_inlet    *x_inlet_fb2;
    t_outlet   *x_outlet1;
    t_outlet   *x_outlet2;
    double      x_sr_rec;
}t_xmod;

double xmod_wrap_phase(double phase){
    while(phase >= 1)
        phase -= 1.;
    while(phase < 0)
        phase += 1.;
    return(phase);
}

void xmod_pm(t_xmod *x){
    x->x_pm = 1;
}

void xmod_fm(t_xmod *x){
    x->x_pm = 0;
}

static t_int *xmod_perform(t_int *w){
    t_xmod *x = (t_xmod *)(w[1]);
    t_float *in1 = (t_float *)(w[2]); // freq1
    t_float *in2 = (t_float *)(w[3]); // index1
    t_float *in3 = (t_float *)(w[4]); // freq2
    t_float *in4 = (t_float *)(w[5]); // index2
    t_float *out1 = (t_float *)(w[6]);
    t_float *out2 = (t_float *)(w[7]);
    float *yn = x->x_yn;
    double *phase1 = x->x_phase1;
    double *phase2 = x->x_phase2;
    float sr_rec = x->x_sr_rec;
    int n = x->x_n, ch2 = x->x_ch2, ch3 = x->x_ch3, ch4 = x->x_ch4;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float freq1 = in1[j*n + i];
            float index1 = ch2 == 1 ? in2[i] : in2[j*n + i];
            float freq2 = ch3 == 1 ? in3[i] : in3[j*n + i];
            float index2 = ch4 == 1 ? in4[i] : in4[j*n + i];
            if(x->x_pm){ // phase modulation
                float output1 = read_sintab(xmod_wrap_phase(phase1[j] + (yn[j] * index2)));
                out1[j*n + i] = output1;
                out2[j*n + i]  = yn[j] = read_sintab(xmod_wrap_phase(phase2[j] + (output1 * index1)));
                phase1[j] += (double)(freq1 * sr_rec);
                phase2[j] += (double)(freq2 * sr_rec);
            }
            else{ // frequency modulation
                float hz1 = freq1 + yn[j] * index2;
                float output1 = read_sintab(phase1[j]);
                out1[j*n + i] = output1;
                float hz2 = freq2 + output1 * index1;
                out2[j*n + i] = yn[j] = read_sintab(phase2[j]);
                phase1[j] += (double)(hz1 * sr_rec);
                phase2[j] += (double)(hz2 * sr_rec);
            }
            phase1[j] = xmod_wrap_phase(phase1[j]);
            phase2[j] = xmod_wrap_phase(phase2[j]);
        }
    }
    x->x_yn = yn; // 1 sample feedback
    x->x_phase1 = phase1;
    x->x_phase2 = phase2;
    return(w+8);
}

static void xmod_dsp(t_xmod *x, t_signal **sp){
    x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    int chs = sp[0]->s_nchans;
    int ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans, ch4 = sp[3]->s_nchans;
    x->x_n = sp[0]->s_n;
    if(x->x_nchans != chs){
        x->x_phase1 = (double *)resizebytes(x->x_phase1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase2 = (double *)resizebytes(x->x_phase2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_yn = (t_float *)resizebytes(x->x_yn,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[4], chs);
    signal_setmultiout(&sp[5], chs);
    if((ch2 > 1 && ch2 != x->x_nchans) || (ch3 > 1 && ch3 != x->x_nchans)
    || (ch4 > 1 && ch4 != x->x_nchans)){
        dsp_add_zero(sp[4]->s_vec, chs*x->x_n);
        dsp_add_zero(sp[5]->s_vec, chs*x->x_n);
        pd_error(x, "[xmod~]: channel sizes mismatch");
        return;
    }
    dsp_add(xmod_perform, 7, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
}

static void *xmod_free(t_xmod *x){
    freebytes(x->x_phase1, x->x_nchans * sizeof(*x->x_phase1));
    freebytes(x->x_phase2, x->x_nchans * sizeof(*x->x_phase2));
    freebytes(x->x_yn, x->x_nchans * sizeof(*x->x_yn));
    inlet_free(x->x_inlet_fb);
    inlet_free(x->x_inlet_freq2);
    inlet_free(x->x_inlet_fb2);
    outlet_free(x->x_outlet1);
    outlet_free(x->x_outlet2);
    return(void *)x;
}

static void *xmod_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_xmod *x = (t_xmod *)pd_new(xmod_class);
    x->x_phase1 = (double *)getbytes(sizeof(*x->x_phase1));
    x->x_phase2 = (double *)getbytes(sizeof(*x->x_phase2));
    x->x_yn = (t_float *)getbytes(sizeof(*x->x_yn));
    x->x_phase1[0] = x->x_phase2[0] = 0.0;
    x->x_yn[0] = 0.0;
    t_float init_freq1 = 0;
    t_float init_fb1 = 0;
    t_float init_freq2 = 0;
    t_float init_fb2 = 0;
    t_int pm = 0;
    t_int argnum = 0;
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            if(!argnum && atom_getsymbolarg(0, ac, av) == gensym("-pm")){
                pm = 1;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else if(av->a_type == A_FLOAT){
            t_float argval = atom_getfloatarg(0, ac, av);
            switch(argnum){
                case 0:
                    init_freq1 = argval;
                    break;
                case 1:
                    init_fb1 = argval;
                    break;
                case 2:
                    init_freq2 = argval;
                    break;
                case 3:
                    init_fb2 = argval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--, av++;
        }
        else
            goto errstate;
    };
    x->x_pm = pm;
    x->x_freq = init_freq1;
    x->x_inlet_fb = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_fb, init_fb1);
    x->x_inlet_freq2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_freq2, init_freq2);
    x->x_inlet_fb2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_fb2, init_fb2);
    x->x_outlet1 = outlet_new(&x->x_obj, &s_signal);
    x->x_outlet2 = outlet_new(&x->x_obj, &s_signal);
    return(x);
errstate:
    pd_error(x, "[xmod~]: improper args");
    return NULL;
}

void xmod_tilde_setup(void){
    xmod_class = class_new(gensym("xmod~"), (t_newmethod)xmod_new, (t_method)xmod_free, sizeof(t_xmod), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(xmod_class, t_xmod, x_freq);
    class_addmethod(xmod_class, (t_method)xmod_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(xmod_class, (t_method)xmod_pm, gensym("pm"), 0);
    class_addmethod(xmod_class, (t_method)xmod_fm, gensym("fm"), 0);
}
