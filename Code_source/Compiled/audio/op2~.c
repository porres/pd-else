// Porres 2023

#include "m_pd.h"
#include "buffer.h"

typedef struct _op2{
    t_object    x_obj;
    float      *x_y1n1;
    float      *x_y1n2;
    float      *x_y2n1;
    float      *x_y2n2;
    double     *x_phase_1;
    double     *x_phase_2;
    t_float     x_freq;
    t_float     x_ratio1;
    t_float     x_ratio2;
    t_float     x_detune1;
    t_float     x_detune2;
    t_float     x_1to1;
    t_float     x_1to2;
    t_float     x_2to1;
    t_float     x_2to2;
    
    t_float     x_fvol1;
    t_float     x_vol1;
    t_float     x_fvol2;
    t_float     x_vol2;
    t_float     x_fpan1;
    t_float     x_pan1;
    t_float     x_fpan2;
    t_float     x_pan2;
    
    t_inlet    *x_inl1;
    t_inlet    *x_inl2;
    int         x_ch2;
    int         x_ch3;
    int         x_n;
    int         x_nchans;
    double      x_sr_rec;
    
    float x_ramp;
}t_op2;

static t_class *op2_class;

static void op2_vol1(t_op2 *x, t_floatarg f){
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    x->x_vol1 = f;
}

static void op2_vol2(t_op2 *x, t_floatarg f){
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    x->x_vol2 = f;
}

static void op2_pan1(t_op2 *x, t_floatarg f){
    f = f < -1 ? -1 : f > 1 ? 1 : f;
    x->x_pan1 = (f * 0.5 + 0.5) * .25;
}

static void op2_pan2(t_op2 *x, t_floatarg f){
    f = f < -1 ? -1 : f > 1 ? 1 : f;
    x->x_pan2 = (f * 0.5 + 0.5) * .25;
}

static void op2_detune1(t_op2 *x, t_floatarg f){
    x->x_detune1 = f;
}

static void op2_detune2(t_op2 *x, t_floatarg f){
    x->x_detune2 = f;
}

static void op2_ratio1(t_op2 *x, t_floatarg f){
    x->x_ratio1 = f;
}

static void op2_ratio2(t_op2 *x, t_floatarg f){
    x->x_ratio2 = f;
}

static void op2_1to1(t_op2 *x, t_floatarg f){
    x->x_1to1 = f;
}

static void op2_1to2(t_op2 *x, t_floatarg f){
    x->x_1to2 = f;
}

static void op2_2to1(t_op2 *x, t_floatarg f){
    x->x_2to1 = f;
}

static void op2_2to2(t_op2 *x, t_floatarg f){
    x->x_2to2 = f;
}

double op2_wrap_phase(double phase){
    while(phase >= 1)
        phase -= 1.;
    while(phase < 0)
        phase += 1.;
    return(phase);
}

static t_int *op2_perform(t_int *w){
    t_op2 *x = (t_op2 *)(w[1]);
    t_float *freq = (t_float *)(w[2]);
    t_float *l1 = (t_float *)(w[3]);
    t_float *l2 = (t_float *)(w[4]);
    t_float *out1 = (t_float *)(w[5]);
    t_float *out2 = (t_float *)(w[6]);
    int n = x->x_n;
    int ch2 = x->x_ch2, ch3 = x->x_ch3;
    float *y1n1 = x->x_y1n1;
    float *y1n2 = x->x_y1n2;
    float *y2n1 = x->x_y2n1;
    float *y2n2 = x->x_y2n2;
    double *ph1 = x->x_phase_1;
    double *ph2 = x->x_phase_2;
    double pan1 = x->x_fpan1, pan2 = x->x_fpan2;
    double vol1 = x->x_fvol1, vol2 = x->x_fvol2;
    
    float p1Inc = (x->x_pan1 - pan1) * x->x_ramp;
    float p2Inc = (x->x_pan2 - pan2) * x->x_ramp;
    float v1Inc = (x->x_vol1 - vol1) * x->x_ramp;
    float v2Inc = (x->x_vol2 - vol2) * x->x_ramp;
    
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double hz = freq[j*n + i];
            float level1 = ch2 == 1 ? l1[i] : l1[j*n + i];
            float level2 = ch3 == 1 ? l2[i] : l2[j*n + i];
            
            float mod1 = ((y1n1[j] + y1n2[j]) * 0.5);
            float op1 = read_sintab(op2_wrap_phase(ph1[j] + mod1));
            float bus1 = op1 * x->x_1to1;
            
            float mod2 = ((y2n1[j] + y2n2[j]) * 0.5);
            mod2 += (op1 * x->x_1to2);
            
            float op2 = read_sintab(op2_wrap_phase(ph2[j] + mod2));
            bus1 += (op2 * x->x_2to1);
            
            double inc1 = (hz + x->x_detune1) * x->x_sr_rec;
            double inc2 = (hz + x->x_detune2) * x->x_sr_rec;
            ph1[j] = op2_wrap_phase(ph1[j] + inc1 * x->x_ratio1); // phase inc
            ph2[j] = op2_wrap_phase(ph2[j] + inc2 * x->x_ratio2); // phase inc
            
            float g1 = op1 * vol1 * level1;
            float g2 = op2 * vol2 * level2;
            float panL = 0;
            panL += (g1 * read_sintab(pan1 + 0.25));
            panL += (g2 * read_sintab(pan2 + 0.25));
            float panR = 0;
            panR += (g1 * read_sintab(pan1));
            panR += (g2 * read_sintab(pan2));
            out1[j*n + i] = panL;
            out2[j*n + i] = panR;
            
            y1n2[j] = y1n1[j];
            y1n1[j] = bus1;
            y2n2[j] = y2n2[j];
            y2n1[j] = op2 * x->x_2to2;
            
            pan1 += p1Inc;
            pan2 += p2Inc;
            vol1 += v1Inc;
            vol2 += v2Inc;
        }
    }
    x->x_y1n1 = y1n1;
    x->x_y1n2 = y1n2;
    x->x_y2n1 = y2n1;
    x->x_y2n2 = y2n2;
    x->x_phase_1 = ph1;
    x->x_phase_2 = ph2;
    x->x_fpan1 = pan1;
    x->x_fpan2 = pan2;
    x->x_fvol1 = vol1;
    x->x_fvol2 = vol2;
    return(w+7);
}

static void op2_dsp(t_op2 *x, t_signal **sp){
    x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    x->x_ramp = 100.f*x->x_sr_rec;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans;
    if((ch2 > 1 && ch2 != chs) || (ch3 > 1 && ch3 != chs)){
        dsp_add_zero(sp[3]->s_vec, chs*x->x_n);
        dsp_add_zero(sp[4]->s_vec, chs*x->x_n);
        pd_error(x, "[op2~]: channel sizes mismatch");
        return;
    }
    x->x_ch2 = ch2;
    x->x_ch3 = ch3;
    signal_setmultiout(&sp[3], chs);
    signal_setmultiout(&sp[4], chs);
    if(x->x_nchans != chs){
        x->x_phase_1 = (double *)resizebytes(x->x_phase_1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase_2 = (double *)resizebytes(x->x_phase_2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_y1n1 = (float *)resizebytes(x->x_y1n1,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y1n2 = (float *)resizebytes(x->x_y1n2,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y2n1 = (float *)resizebytes(x->x_y2n1,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y2n2 = (float *)resizebytes(x->x_y2n2,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_nchans = chs;
    }
    dsp_add(op2_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
        sp[3]->s_vec, sp[4]->s_vec);
}

static void *op2_free(t_op2 *x){
    inlet_free(x->x_inl1);
    inlet_free(x->x_inl2);
    freebytes(x->x_phase_1, x->x_nchans * sizeof(*x->x_phase_1));
    freebytes(x->x_phase_2, x->x_nchans * sizeof(*x->x_phase_2));
    freebytes(x->x_y1n1, x->x_nchans * sizeof(*x->x_y1n1));
    freebytes(x->x_y1n2, x->x_nchans * sizeof(*x->x_y1n2));
    freebytes(x->x_y2n1, x->x_nchans * sizeof(*x->x_y2n1));
    freebytes(x->x_y2n2, x->x_nchans * sizeof(*x->x_y2n2));
    return(void *)x;
}

static void *op2_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_op2 *x = (t_op2 *)pd_new(op2_class);
    x->x_phase_1 = (double *)getbytes(sizeof(*x->x_phase_1));
    x->x_phase_2 = (double *)getbytes(sizeof(*x->x_phase_2));
    x->x_y1n1 = (float *)getbytes(sizeof(*x->x_y1n1));
    x->x_y1n2 = (float *)getbytes(sizeof(*x->x_y1n2));
    x->x_y2n1 = (float *)getbytes(sizeof(*x->x_y2n1));
    x->x_y2n2 = (float *)getbytes(sizeof(*x->x_y2n2));
    init_sine_table();
    x->x_ratio1 = x->x_ratio2 = 1;
    x->x_vol1 = x->x_fvol1 = x->x_fvol2 = x->x_vol2 = 1;
    x->x_pan1 = x->x_fpan1 = x->x_fpan2 = x->x_pan2 = .125;
    while(ac){
        if(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-ratio")){
                if(ac < 3)
                    goto errstate;
                ac--, av++;
                x->x_ratio1 = atom_getfloat(av);
                ac--, av++;
                x->x_ratio2 = atom_getfloat(av);
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-detune")){
                if(ac < 3)
                    goto errstate;
                ac--, av++;
                x->x_detune1 = atom_getfloat(av);
                ac--, av++;
                x->x_detune2 = atom_getfloat(av);
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-idx")){
                if(ac < 5)
                    goto errstate;
                ac--, av++;
                x->x_1to1 = atom_getfloat(av);
                ac--, av++;
                x->x_2to1 = atom_getfloat(av);
                ac--, av++;
                x->x_1to2 = atom_getfloat(av);
                ac--, av++;
                x->x_2to2 = atom_getfloat(av);
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-vol")){
                if(ac < 3)
                    goto errstate;
                ac--, av++;
                x->x_vol1 = atom_getfloat(av);
                ac--, av++;
                x->x_vol2 = atom_getfloat(av);
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-pan")){
                if(ac < 3)
                    goto errstate;
                ac--, av++;
                x->x_pan1 = atom_getfloat(av);
                ac--, av++;
                x->x_pan2 = atom_getfloat(av);
                ac--, av++;
            }
            else
                goto errstate;
        }
        else if(av->a_type == A_FLOAT){
            if(ac > 1)
                goto errstate;
            x->x_freq = atom_getfloat(av);
            ac--, av++;
        }
    }
    x->x_inl1 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inl1, 1);
    x->x_inl2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inl2, 1);
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
errstate:
    pd_error(x, "[op2~]: improper args");
    return(NULL);
}

void op2_tilde_setup(void){
    op2_class = class_new(gensym("op2~"), (t_newmethod)op2_new, (t_method)op2_free, sizeof(t_op2), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(op2_class, t_op2, x_freq);
    class_addmethod(op2_class, (t_method)op2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(op2_class, (t_method)op2_1to1, gensym("1to1"), A_FLOAT, 0);
    class_addmethod(op2_class, (t_method)op2_1to2, gensym("1to2"), A_FLOAT, 0);
    class_addmethod(op2_class, (t_method)op2_2to1, gensym("2to1"), A_FLOAT, 0);
    class_addmethod(op2_class, (t_method)op2_2to2, gensym("2to2"), A_FLOAT, 0);
    class_addmethod(op2_class, (t_method)op2_ratio1, gensym("ratio1"), A_FLOAT, 0);
    class_addmethod(op2_class, (t_method)op2_ratio2, gensym("ratio2"), A_FLOAT, 0);
    class_addmethod(op2_class, (t_method)op2_detune1, gensym("detune1"), A_FLOAT, 0);
    class_addmethod(op2_class, (t_method)op2_detune2, gensym("detune2"), A_FLOAT, 0);
    class_addmethod(op2_class, (t_method)op2_vol1, gensym("vol1"), A_FLOAT, 0);
    class_addmethod(op2_class, (t_method)op2_vol2, gensym("vol2"), A_FLOAT, 0);
    class_addmethod(op2_class, (t_method)op2_pan1, gensym("pan1"), A_FLOAT, 0);
    class_addmethod(op2_class, (t_method)op2_pan2, gensym("pan2"), A_FLOAT, 0);
}