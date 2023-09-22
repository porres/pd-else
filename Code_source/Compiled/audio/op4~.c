// Porres 2023

#include "m_pd.h"
#include "buffer.h"

typedef struct _op4{
    t_object    x_obj;
    float      *x_y1n1;
    float      *x_y1n2;
    float      *x_y2n1;
    float      *x_y2n2;
    float      *x_y3n1;
    float      *x_y3n2;
    float      *x_y4n1;
    float      *x_y4n2;
    double     *x_phase_1;
    double     *x_phase_2;
    double     *x_phase_3;
    double     *x_phase_4;
    t_float     x_freq;
    t_float     x_ratio1;
    t_float     x_ratio2;
    t_float     x_ratio3;
    t_float     x_ratio4;
    t_float     x_detune1;
    t_float     x_detune2;
    t_float     x_detune3;
    t_float     x_detune4;
    t_float     x_1to1;
    t_float     x_1to2;
    t_float     x_1to3;
    t_float     x_1to4;
    t_float     x_2to1;
    t_float     x_2to2;
    t_float     x_2to3;
    t_float     x_2to4;
    t_float     x_3to1;
    t_float     x_3to2;
    t_float     x_3to3;
    t_float     x_3to4;
    t_float     x_4to1;
    t_float     x_4to2;
    t_float     x_4to3;
    t_float     x_4to4;
    t_float     x_fvol1;
    t_float     x_vol1;
    t_float     x_fvol2;
    t_float     x_vol2;
    t_float     x_fvol3;
    t_float     x_vol3;
    t_float     x_fvol4;
    t_float     x_vol4;
    t_float     x_fpan1;
    t_float     x_pan1;
    t_float     x_fpan2;
    t_float     x_pan2;
    t_float     x_fpan3;
    t_float     x_pan3;
    t_float     x_fpan4;
    t_float     x_pan4;
    t_inlet    *x_inl1;
    t_inlet    *x_inl2;
    t_inlet    *x_inl3;
    t_inlet    *x_inl4;
    int         x_nchans;
    int         x_n;
    int         x_ch2;
    int         x_ch3;
    int         x_ch4;
    int         x_ch5;
    double      x_sr_rec;
    float       x_ramp;
}t_op4;

static t_class *op4_class;

static void op4_vol1(t_op4 *x, t_floatarg f){
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    x->x_vol1 = f;
}

static void op4_vol2(t_op4 *x, t_floatarg f){
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    x->x_vol2 = f;
}

static void op4_vol3(t_op4 *x, t_floatarg f){
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    x->x_vol3 = f;
}

static void op4_vol4(t_op4 *x, t_floatarg f){
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    x->x_vol4 = f;
}

static void op4_vol(t_op4 *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 4){
        op4_vol1(x, atom_getfloat(av));
        av++;
        op4_vol2(x, atom_getfloat(av));
        av++;
        op4_vol3(x, atom_getfloat(av));
        av++;
        op4_vol4(x, atom_getfloat(av));
    }
}

static void op4_pan1(t_op4 *x, t_floatarg f){
    f = f < -1 ? -1 : f > 1 ? 1 : f;
    x->x_pan1 = (f * 0.5 + 0.5) * .25;
}

static void op4_pan2(t_op4 *x, t_floatarg f){
    f = f < -1 ? -1 : f > 1 ? 1 : f;
    x->x_pan2 = (f * 0.5 + 0.5) * .25;
}

static void op4_pan3(t_op4 *x, t_floatarg f){
    f = f < -1 ? -1 : f > 1 ? 1 : f;
    x->x_pan3 = (f * 0.5 + 0.5) * .25;
}

static void op4_pan4(t_op4 *x, t_floatarg f){
    f = f < -1 ? -1 : f > 1 ? 1 : f;
    x->x_pan4 = (f * 0.5 + 0.5) * .25;
}

static void op4_pan(t_op4 *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 4){
        op4_pan1(x, atom_getfloat(av));
        av++;
        op4_pan2(x, atom_getfloat(av));
        av++;
        op4_pan3(x, atom_getfloat(av));
        av++;
        op4_pan4(x, atom_getfloat(av));
    }
}

static void op4_detune1(t_op4 *x, t_floatarg f){
    x->x_detune1 = f;
}

static void op4_detune2(t_op4 *x, t_floatarg f){
    x->x_detune2 = f;
}

static void op4_detune3(t_op4 *x, t_floatarg f){
    x->x_detune3 = f;
}

static void op4_detune4(t_op4 *x, t_floatarg f){
    x->x_detune4 = f;
}

static void op4_detune(t_op4 *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 4){
        op4_detune1(x, atom_getfloat(av));
        av++;
        op4_detune2(x, atom_getfloat(av));
        av++;
        op4_detune3(x, atom_getfloat(av));
        av++;
        op4_detune4(x, atom_getfloat(av));
    }
}

static void op4_ratio1(t_op4 *x, t_floatarg f){
    x->x_ratio1 = f;
}

static void op4_ratio2(t_op4 *x, t_floatarg f){
    x->x_ratio2 = f;
}

static void op4_ratio3(t_op4 *x, t_floatarg f){
    x->x_ratio3 = f;
}

static void op4_ratio4(t_op4 *x, t_floatarg f){
    x->x_ratio4 = f;
}

static void op4_ratio(t_op4 *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 4){
        op4_ratio1(x, atom_getfloat(av));
        av++;
        op4_ratio2(x, atom_getfloat(av));
        av++;
        op4_ratio3(x, atom_getfloat(av));
        av++;
        op4_ratio4(x, atom_getfloat(av));
    }
}

static void op4_1to1(t_op4 *x, t_floatarg f){
    x->x_1to1 = f;
}

static void op4_1to2(t_op4 *x, t_floatarg f){
    x->x_1to2 = f;
}

static void op4_1to3(t_op4 *x, t_floatarg f){
    x->x_1to3 = f;
}

static void op4_1to4(t_op4 *x, t_floatarg f){
    x->x_1to4 = f;
}

static void op4_2to1(t_op4 *x, t_floatarg f){
    x->x_2to1 = f;
}

static void op4_2to2(t_op4 *x, t_floatarg f){
    x->x_2to2 = f;
}

static void op4_2to3(t_op4 *x, t_floatarg f){
    x->x_2to3 = f;
}

static void op4_2to4(t_op4 *x, t_floatarg f){
    x->x_2to4 = f;
}

static void op4_3to1(t_op4 *x, t_floatarg f){
    x->x_3to1 = f;
}

static void op4_3to2(t_op4 *x, t_floatarg f){
    x->x_3to2 = f;
}

static void op4_3to3(t_op4 *x, t_floatarg f){
    x->x_3to3 = f;
}

static void op4_3to4(t_op4 *x, t_floatarg f){
    x->x_3to4 = f;
}

static void op4_4to1(t_op4 *x, t_floatarg f){
    x->x_4to1 = f;
}

static void op4_4to2(t_op4 *x, t_floatarg f){
    x->x_4to2 = f;
}

static void op4_4to3(t_op4 *x, t_floatarg f){
    x->x_4to3 = f;
}

static void op4_4to4(t_op4 *x, t_floatarg f){
    x->x_4to4 = f;
}

static void op4_idx(t_op4 *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 16){
        x->x_1to1 = atom_getfloat(av);
        av++;
        x->x_2to1 = atom_getfloat(av);
        av++;
        x->x_3to1 = atom_getfloat(av);
        av++;
        x->x_4to1 = atom_getfloat(av);
        av++;
        x->x_1to2 = atom_getfloat(av);
        av++;
        x->x_2to2 = atom_getfloat(av);
        av++;
        x->x_3to2 = atom_getfloat(av);
        av++;
        x->x_4to2 = atom_getfloat(av);
        av++;
        x->x_1to3 = atom_getfloat(av);
        av++;
        x->x_2to3 = atom_getfloat(av);
        av++;
        x->x_3to3 = atom_getfloat(av);
        av++;
        x->x_4to3 = atom_getfloat(av);
        av++;
        x->x_1to4 = atom_getfloat(av);
        av++;
        x->x_2to4 = atom_getfloat(av);
        av++;
        x->x_3to4 = atom_getfloat(av);
        av++;
        x->x_4to4 = atom_getfloat(av);
        av++;
    }
}

double op4_wrap_phase(double phase){
    while(phase >= 1)
        phase -= 1.;
    while(phase < 0)
        phase += 1.;
    return(phase);
}

static t_int *op4_perform(t_int *w){
    t_op4 *x = (t_op4 *)(w[1]);
    t_float *freq = (t_float *)(w[2]);
    t_float *l1 = (t_float *)(w[3]);
    t_float *l2 = (t_float *)(w[4]);
    t_float *l3 = (t_float *)(w[5]);
    t_float *l4 = (t_float *)(w[6]);
    t_float *out1 = (t_float *)(w[7]);
    t_float *out2 = (t_float *)(w[8]);
    int n = x->x_n, ch2 = x->x_ch2, ch3 = x->x_ch3;
    int ch4 = x->x_ch4, ch5 = x->x_ch5;
    float *y1n1 = x->x_y1n1;
    float *y1n2 = x->x_y1n2;
    float *y2n1 = x->x_y2n1;
    float *y2n2 = x->x_y2n2;
    float *y3n1 = x->x_y3n1;
    float *y3n2 = x->x_y3n2;
    float *y4n1 = x->x_y4n1;
    float *y4n2 = x->x_y4n2;
    double *ph1 = x->x_phase_1;
    double *ph2 = x->x_phase_2;
    double *ph3 = x->x_phase_3;
    double *ph4 = x->x_phase_4;
    double pan1 = x->x_fpan1, pan2 = x->x_fpan2;
    double pan3 = x->x_fpan3, pan4 = x->x_fpan4;
    double vol1 = x->x_fvol1, vol2 = x->x_fvol2;
    double vol3 = x->x_fvol3, vol4 = x->x_fvol4;
    float p1Inc = (x->x_pan1 - pan1) * x->x_ramp;
    float p2Inc = (x->x_pan2 - pan2) * x->x_ramp;
    float p3Inc = (x->x_pan3 - pan3) * x->x_ramp;
    float p4Inc = (x->x_pan4 - pan4) * x->x_ramp;
    float v1Inc = (x->x_vol1 - vol1) * x->x_ramp;
    float v2Inc = (x->x_vol2 - vol2) * x->x_ramp;
    float v3Inc = (x->x_vol3 - vol3) * x->x_ramp;
    float v4Inc = (x->x_vol4 - vol4) * x->x_ramp;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double hz = freq[j*n + i];
            float level1 = ch2 == 1 ? l1[i] : l1[j*n + i];
            float level2 = ch3 == 1 ? l2[i] : l2[j*n + i];
            float level3 = ch4 == 1 ? l3[i] : l3[j*n + i];
            float level4 = ch5 == 1 ? l4[i] : l4[j*n + i];
            
            float mod1 = ((y1n1[j] + y1n2[j]) * 0.5); // fb bus1
            float op1 = read_sintab(op4_wrap_phase(ph1[j] + mod1));
            float bus1 = op1 * x->x_1to1;
            
            float mod2 = ((y2n1[j] + y2n2[j]) * 0.5); // fb bus2
            mod2 += (op1 * x->x_1to2); // ff
            float op2 = read_sintab(op4_wrap_phase(ph2[j] + mod2));
            bus1 += (op2 * x->x_2to1);
            float bus2 = (op2 * x->x_2to2);
            
            float mod3 = ((y3n1[j] + y3n2[j]) * 0.5); // fb bus3
            mod3 += (op1 * x->x_1to3);
            mod3 += (op2 * x->x_2to3);
            float op3 = read_sintab(op4_wrap_phase(ph3[j] + mod3));;
            bus1 += (op3 * x->x_3to1);
            bus2 += (op3 * x->x_3to2);
            float bus3 = op3 * x->x_3to3;
            
            float mod4 = ((y4n1[j] + y4n2[j]) * 0.5); // fb bus4
            mod4 += (op1 * x->x_1to4);
            mod4 += (op2 * x->x_2to4);
            mod4 += (op3 * x->x_3to4);
            float op4 = read_sintab(op4_wrap_phase(ph4[j] + mod4));
            bus1 += (op4 * x->x_4to1);
            bus2 += (op4 * x->x_4to2);
            bus3 += (op4 * x->x_4to3);
            float bus4 = (op4 * x->x_4to4);
            
            double inc1 = (hz + x->x_detune1) * x->x_sr_rec;
            double inc2 = (hz + x->x_detune2) * x->x_sr_rec;
            double inc3 = (hz + x->x_detune3) * x->x_sr_rec;
            double inc4 = (hz + x->x_detune4) * x->x_sr_rec;
            ph1[j] = op4_wrap_phase(ph1[j] + inc1 * x->x_ratio1); // phase inc
            ph2[j] = op4_wrap_phase(ph2[j] + inc2 * x->x_ratio2); // phase inc
            ph3[j] = op4_wrap_phase(ph3[j] + inc3 * x->x_ratio3); // phase inc
            ph4[j] = op4_wrap_phase(ph4[j] + inc4 * x->x_ratio4); // phase inc
            
            float g1 = op1 * vol1 * level1;
            float g2 = op2 * vol2 * level2;
            float g3 = op3 * vol3 * level3;
            float g4 = op4 * vol4 * level4;
            float panL = 0;
            panL += (g1 * read_sintab(pan1 + 0.25));
            panL += (g2 * read_sintab(pan2 + 0.25));
            panL += (g3 * read_sintab(pan3 + 0.25));
            panL += (g4 * read_sintab(pan4 + 0.25));
            float panR = 0;
            panR += (g1 * read_sintab(pan1));
            panR += (g2 * read_sintab(pan2));
            panR += (g3 * read_sintab(pan3));
            panR += (g4 * read_sintab(pan4));
            out1[j*n + i] = panL;
            out2[j*n + i] = panR;
            
            y1n2[j] = y1n1[j];
            y1n1[j] = bus1;
            y2n2[j] = y2n1[j];
            y2n1[j] = bus2;
            y3n2[j] = y3n1[j];
            y3n1[j] = bus3;
            y4n2[j] = y4n1[j];
            y4n1[j] = bus4;
            
            pan1 += p1Inc;
            pan2 += p2Inc;
            pan3 += p3Inc;
            pan4 += p4Inc;
            vol1 += v1Inc;
            vol2 += v2Inc;
            vol3 += v3Inc;
            vol4 += v4Inc;
        }
    }
    x->x_y1n1 = y1n1;
    x->x_y1n2 = y1n2;
    x->x_y2n1 = y2n1;
    x->x_y2n2 = y2n2;
    x->x_y3n1 = y3n1;
    x->x_y3n2 = y3n2;
    x->x_y4n1 = y4n1;
    x->x_y4n2 = y4n2;
    x->x_phase_1 = ph1;
    x->x_phase_2 = ph2;
    x->x_phase_3 = ph3;
    x->x_phase_4 = ph4;
    x->x_fpan1 = pan1;
    x->x_fpan2 = pan2;
    x->x_fpan3 = pan3;
    x->x_fpan4 = pan4;
    x->x_fvol1 = vol1;
    x->x_fvol2 = vol2;
    x->x_fvol3 = vol3;
    x->x_fvol4 = vol4;
    return(w+9);
}

static void op4_dsp(t_op4 *x, t_signal **sp){
    x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    x->x_ramp = 100.f*x->x_sr_rec;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans;
    int ch4 = sp[3]->s_nchans, ch5 = sp[4]->s_nchans;
    if((ch2 > 1 && ch2 != chs) || (ch3 > 1 && ch3 != chs)
    || (ch4 > 1 && ch4 != chs) || (ch5 > 1 && ch5 != chs)){
        dsp_add_zero(sp[5]->s_vec, chs*x->x_n);
        dsp_add_zero(sp[6]->s_vec, chs*x->x_n);
        pd_error(x, "[op4~]: channel sizes mismatch");
        return;
    }
    x->x_ch2 = ch2;
    x->x_ch3 = ch3;
    x->x_ch4 = ch4;
    x->x_ch5 = ch5;
    signal_setmultiout(&sp[5], chs);
    signal_setmultiout(&sp[6], chs);
    if(x->x_nchans != chs){
        x->x_phase_1 = (double *)resizebytes(x->x_phase_1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase_2 = (double *)resizebytes(x->x_phase_2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase_3 = (double *)resizebytes(x->x_phase_3,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase_4 = (double *)resizebytes(x->x_phase_4,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_y1n1 = (float *)resizebytes(x->x_y1n1,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y1n2 = (float *)resizebytes(x->x_y1n2,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y2n1 = (float *)resizebytes(x->x_y2n1,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y2n2 = (float *)resizebytes(x->x_y2n2,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y3n1 = (float *)resizebytes(x->x_y3n1,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y3n2 = (float *)resizebytes(x->x_y3n2,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y4n1 = (float *)resizebytes(x->x_y4n1,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y4n2 = (float *)resizebytes(x->x_y4n2,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_nchans = chs;
    }
    dsp_add(op4_perform, 8, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
        sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec);
}

static void *op4_free(t_op4 *x){
    inlet_free(x->x_inl1);
    inlet_free(x->x_inl2);
    inlet_free(x->x_inl3);
    inlet_free(x->x_inl4);
    freebytes(x->x_phase_1, x->x_nchans * sizeof(*x->x_phase_1));
    freebytes(x->x_phase_2, x->x_nchans * sizeof(*x->x_phase_2));
    freebytes(x->x_phase_3, x->x_nchans * sizeof(*x->x_phase_3));
    freebytes(x->x_phase_4, x->x_nchans * sizeof(*x->x_phase_4));
    freebytes(x->x_y1n1, x->x_nchans * sizeof(*x->x_y1n1));
    freebytes(x->x_y1n2, x->x_nchans * sizeof(*x->x_y1n2));
    freebytes(x->x_y2n1, x->x_nchans * sizeof(*x->x_y2n1));
    freebytes(x->x_y2n2, x->x_nchans * sizeof(*x->x_y2n2));
    freebytes(x->x_y3n1, x->x_nchans * sizeof(*x->x_y3n1));
    freebytes(x->x_y3n2, x->x_nchans * sizeof(*x->x_y3n2));
    freebytes(x->x_y4n1, x->x_nchans * sizeof(*x->x_y4n1));
    freebytes(x->x_y4n2, x->x_nchans * sizeof(*x->x_y4n2));
    return(void *)x;
}

static void *op4_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_op4 *x = (t_op4 *)pd_new(op4_class);
    x->x_phase_1 = (double *)getbytes(sizeof(*x->x_phase_1));
    x->x_phase_2 = (double *)getbytes(sizeof(*x->x_phase_2));
    x->x_phase_3 = (double *)getbytes(sizeof(*x->x_phase_3));
    x->x_phase_4 = (double *)getbytes(sizeof(*x->x_phase_4));
    x->x_y1n1 = (float *)getbytes(sizeof(*x->x_y1n1));
    x->x_y1n2 = (float *)getbytes(sizeof(*x->x_y1n2));
    x->x_y2n1 = (float *)getbytes(sizeof(*x->x_y2n1));
    x->x_y2n2 = (float *)getbytes(sizeof(*x->x_y2n2));
    x->x_y3n1 = (float *)getbytes(sizeof(*x->x_y3n1));
    x->x_y3n2 = (float *)getbytes(sizeof(*x->x_y3n2));
    x->x_y4n1 = (float *)getbytes(sizeof(*x->x_y4n1));
    x->x_y4n2 = (float *)getbytes(sizeof(*x->x_y4n2));
    init_sine_table();
    x->x_ratio1 = x->x_ratio2 = x->x_ratio3 = x->x_ratio4 = 1;
    x->x_vol1 = x->x_fvol1 = x->x_fvol2 = x->x_vol2 = 1;
    x->x_vol3 = x->x_fvol3 = x->x_fvol4 = x->x_vol4 = 1;
    x->x_pan1 = x->x_fpan1 = x->x_fpan2 = x->x_pan2 = .125;
    x->x_pan3 = x->x_fpan3 = x->x_fpan4 = x->x_pan4 = .125;
    while(ac){
        if(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-ratio")){
                if(ac < 5)
                    goto errstate;
                ac--, av++;
                x->x_ratio1 = atom_getfloat(av);
                ac--, av++;
                x->x_ratio2 = atom_getfloat(av);
                ac--, av++;
                x->x_ratio3 = atom_getfloat(av);
                ac--, av++;
                x->x_ratio4 = atom_getfloat(av);
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-detune")){
                if(ac < 5)
                    goto errstate;
                ac--, av++;
                x->x_detune1 = atom_getfloat(av);
                ac--, av++;
                x->x_detune2 = atom_getfloat(av);
                ac--, av++;
                x->x_detune3 = atom_getfloat(av);
                ac--, av++;
                x->x_detune4 = atom_getfloat(av);
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-idx")){
                if(ac < 17)
                    goto errstate;
                ac--, av++;
                x->x_1to1 = atom_getfloat(av);
                ac--, av++;
                x->x_2to1 = atom_getfloat(av);
                ac--, av++;
                x->x_3to1 = atom_getfloat(av);
                ac--, av++;
                x->x_4to1 = atom_getfloat(av);
                ac--, av++;
                x->x_1to2 = atom_getfloat(av);
                ac--, av++;
                x->x_2to2 = atom_getfloat(av);
                ac--, av++;
                x->x_3to2 = atom_getfloat(av);
                ac--, av++;
                x->x_4to2 = atom_getfloat(av);
                ac--, av++;
                x->x_1to3 = atom_getfloat(av);
                ac--, av++;
                x->x_2to3 = atom_getfloat(av);
                ac--, av++;
                x->x_3to3 = atom_getfloat(av);
                ac--, av++;
                x->x_4to3 = atom_getfloat(av);
                ac--, av++;
                x->x_1to4 = atom_getfloat(av);
                ac--, av++;
                x->x_2to4 = atom_getfloat(av);
                ac--, av++;
                x->x_3to4 = atom_getfloat(av);
                ac--, av++;
                x->x_4to4 = atom_getfloat(av);
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-vol")){
                if(ac < 5)
                    goto errstate;
                ac--, av++;
                op4_vol1(x, atom_getfloat(av));
                ac--, av++;
                op4_vol2(x, atom_getfloat(av));
                ac--, av++;
                op4_vol3(x, atom_getfloat(av));
                ac--, av++;
                op4_vol4(x, atom_getfloat(av));
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-pan")){
                if(ac < 5)
                    goto errstate;
                ac--, av++;
                op4_pan1(x, atom_getfloat(av));
                ac--, av++;
                op4_pan2(x, atom_getfloat(av));
                ac--, av++;
                op4_pan3(x, atom_getfloat(av));
                ac--, av++;
                op4_pan4(x, atom_getfloat(av));
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
    x->x_inl3 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inl3, 1);
    x->x_inl4 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inl4, 1);
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
errstate:
    pd_error(x, "[op4~]: improper args");
    return(NULL);
}

void op4_tilde_setup(void){
    op4_class = class_new(gensym("op4~"), (t_newmethod)op4_new, (t_method)op4_free, sizeof(t_op4), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(op4_class, t_op4, x_freq);
    class_addmethod(op4_class, (t_method)op4_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(op4_class, (t_method)op4_idx, gensym("idx"), A_GIMME, 0);
    class_addmethod(op4_class, (t_method)op4_1to1, gensym("1to1"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_1to2, gensym("1to2"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_1to3, gensym("1to3"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_1to4, gensym("1to4"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_2to1, gensym("2to1"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_2to2, gensym("2to2"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_2to3, gensym("2to3"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_2to4, gensym("2to4"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_3to1, gensym("3to1"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_3to2, gensym("3to2"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_3to3, gensym("3to3"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_3to4, gensym("3to4"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_4to1, gensym("4to1"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_4to2, gensym("4to2"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_4to3, gensym("4to3"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_4to4, gensym("4to4"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_ratio, gensym("ratio"), A_GIMME, 0);
    class_addmethod(op4_class, (t_method)op4_ratio1, gensym("ratio1"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_ratio2, gensym("ratio2"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_ratio3, gensym("ratio3"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_ratio4, gensym("ratio4"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_detune, gensym("detune"), A_GIMME, 0);
    class_addmethod(op4_class, (t_method)op4_detune1, gensym("detune1"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_detune2, gensym("detune2"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_detune3, gensym("detune3"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_detune4, gensym("detune4"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_vol, gensym("vol"), A_GIMME, 0);
    class_addmethod(op4_class, (t_method)op4_vol1, gensym("vol1"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_vol2, gensym("vol2"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_vol3, gensym("vol3"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_vol4, gensym("vol4"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_pan, gensym("pan"), A_GIMME, 0);
    class_addmethod(op4_class, (t_method)op4_pan1, gensym("pan1"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_pan2, gensym("pan2"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_pan3, gensym("pan3"), A_FLOAT, 0);
    class_addmethod(op4_class, (t_method)op4_pan4, gensym("pan4"), A_FLOAT, 0);
}
