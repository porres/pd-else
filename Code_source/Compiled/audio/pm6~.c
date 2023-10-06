// Porres 2023

#include "m_pd.h"
#include "buffer.h"

typedef struct _pm6{
    t_object    x_obj;
    float      *x_y1n1;
    float      *x_y1n2;
    float      *x_y2n1;
    float      *x_y2n2;
    float      *x_y3n1;
    float      *x_y3n2;
    float      *x_y4n1;
    float      *x_y4n2;
    float      *x_y5n1;
    float      *x_y5n2;
    float      *x_y6n1;
    float      *x_y6n2;
    double     *x_phase_1;
    double     *x_phase_2;
    double     *x_phase_3;
    double     *x_phase_4;
    double     *x_phase_5;
    double     *x_phase_6;
    t_float     x_freq;
    t_float     x_ratio1;
    t_float     x_ratio2;
    t_float     x_ratio3;
    t_float     x_ratio4;
    t_float     x_ratio5;
    t_float     x_ratio6;
    t_float     x_detune1;
    t_float     x_detune2;
    t_float     x_detune3;
    t_float     x_detune4;
    t_float     x_detune5;
    t_float     x_detune6;
    t_float     x_1to1;
    t_float     x_1to2;
    t_float     x_1to3;
    t_float     x_1to4;
    t_float     x_1to5;
    t_float     x_1to6;
    t_float     x_2to1;
    t_float     x_2to2;
    t_float     x_2to3;
    t_float     x_2to4;
    t_float     x_2to5;
    t_float     x_2to6;
    t_float     x_3to1;
    t_float     x_3to2;
    t_float     x_3to3;
    t_float     x_3to4;
    t_float     x_3to5;
    t_float     x_3to6;
    t_float     x_4to1;
    t_float     x_4to2;
    t_float     x_4to3;
    t_float     x_4to4;
    t_float     x_4to5;
    t_float     x_4to6;
    t_float     x_5to1;
    t_float     x_5to2;
    t_float     x_5to3;
    t_float     x_5to4;
    t_float     x_5to5;
    t_float     x_5to6;
    t_float     x_6to1;
    t_float     x_6to2;
    t_float     x_6to3;
    t_float     x_6to4;
    t_float     x_6to5;
    t_float     x_6to6;
    t_float     x_fvol1;
    t_float     x_vol1;
    t_float     x_fvol2;
    t_float     x_vol2;
    t_float     x_fvol3;
    t_float     x_vol3;
    t_float     x_fvol4;
    t_float     x_vol4;
    t_float     x_fvol5;
    t_float     x_vol5;
    t_float     x_fvol6;
    t_float     x_vol6;
    t_float     x_fpan1;
    t_float     x_pan1;
    t_float     x_fpan2;
    t_float     x_pan2;
    t_float     x_fpan3;
    t_float     x_pan3;
    t_float     x_fpan4;
    t_float     x_pan4;
    t_float     x_fpan5;
    t_float     x_pan5;
    t_float     x_fpan6;
    t_float     x_pan6;
    t_inlet    *x_inl1;
    t_inlet    *x_inl2;
    t_inlet    *x_inl3;
    t_inlet    *x_inl4;
    t_inlet    *x_inl5;
    t_inlet    *x_inl6;
    int         x_nchans;
    int         x_n;
    int         x_ch2;
    int         x_ch3;
    int         x_ch4;
    int         x_ch5;
    int         x_ch6;
    int         x_ch7;
    double      x_sr_rec;
    double      x_ramp;
}t_pm6;

static t_class *pm6_class;

static void pm6_vol1(t_pm6 *x, t_floatarg f){
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    x->x_vol1 = f;
}

static void pm6_vol2(t_pm6 *x, t_floatarg f){
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    x->x_vol2 = f;
}

static void pm6_vol3(t_pm6 *x, t_floatarg f){
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    x->x_vol3 = f;
}

static void pm6_vol4(t_pm6 *x, t_floatarg f){
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    x->x_vol4 = f;
}

static void pm6_vol5(t_pm6 *x, t_floatarg f){
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    x->x_vol5 = f;
}

static void pm6_vol6(t_pm6 *x, t_floatarg f){
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    x->x_vol6 = f;
}

static void pm6_vol(t_pm6 *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 6){
        pm6_vol1(x, atom_getfloat(av));
        av++;
        pm6_vol2(x, atom_getfloat(av));
        av++;
        pm6_vol3(x, atom_getfloat(av));
        av++;
        pm6_vol4(x, atom_getfloat(av));
        av++;
        pm6_vol5(x, atom_getfloat(av));
        av++;
        pm6_vol6(x, atom_getfloat(av));
    }
}

static void pm6_pan1(t_pm6 *x, t_floatarg f){
    f = f < -1 ? -1 : f > 1 ? 1 : f;
    x->x_pan1 = (f * 0.5 + 0.5) * .25;
}

static void pm6_pan2(t_pm6 *x, t_floatarg f){
    f = f < -1 ? -1 : f > 1 ? 1 : f;
    x->x_pan2 = (f * 0.5 + 0.5) * .25;
}

static void pm6_pan3(t_pm6 *x, t_floatarg f){
    f = f < -1 ? -1 : f > 1 ? 1 : f;
    x->x_pan3 = (f * 0.5 + 0.5) * .25;
}

static void pm6_pan4(t_pm6 *x, t_floatarg f){
    f = f < -1 ? -1 : f > 1 ? 1 : f;
    x->x_pan4 = (f * 0.5 + 0.5) * .25;
}

static void pm6_pan5(t_pm6 *x, t_floatarg f){
    f = f < -1 ? -1 : f > 1 ? 1 : f;
    x->x_pan5 = (f * 0.5 + 0.5) * .25;
}

static void pm6_pan6(t_pm6 *x, t_floatarg f){
    f = f < -1 ? -1 : f > 1 ? 1 : f;
    x->x_pan6 = (f * 0.5 + 0.5) * .25;
}

static void pm6_pan(t_pm6 *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 6){
        pm6_pan1(x, atom_getfloat(av));
        av++;
        pm6_pan2(x, atom_getfloat(av));
        av++;
        pm6_pan3(x, atom_getfloat(av));
        av++;
        pm6_pan4(x, atom_getfloat(av));
        av++;
        pm6_pan5(x, atom_getfloat(av));
        av++;
        pm6_pan6(x, atom_getfloat(av));
    }
}

static void pm6_detune1(t_pm6 *x, t_floatarg f){
    x->x_detune1 = f;
}

static void pm6_detune2(t_pm6 *x, t_floatarg f){
    x->x_detune2 = f;
}

static void pm6_detune3(t_pm6 *x, t_floatarg f){
    x->x_detune3 = f;
}

static void pm6_detune4(t_pm6 *x, t_floatarg f){
    x->x_detune4 = f;
}

static void pm6_detune5(t_pm6 *x, t_floatarg f){
    x->x_detune5 = f;
}

static void pm6_detune6(t_pm6 *x, t_floatarg f){
    x->x_detune6 = f;
}

static void pm6_detune(t_pm6 *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 6){
        pm6_detune1(x, atom_getfloat(av));
        av++;
        pm6_detune2(x, atom_getfloat(av));
        av++;
        pm6_detune3(x, atom_getfloat(av));
        av++;
        pm6_detune4(x, atom_getfloat(av));
        av++;
        pm6_detune5(x, atom_getfloat(av));
        av++;
        pm6_detune6(x, atom_getfloat(av));
    }
}

static void pm6_ratio1(t_pm6 *x, t_floatarg f){
    x->x_ratio1 = f;
}

static void pm6_ratio2(t_pm6 *x, t_floatarg f){
    x->x_ratio2 = f;
}

static void pm6_ratio3(t_pm6 *x, t_floatarg f){
    x->x_ratio3 = f;
}

static void pm6_ratio4(t_pm6 *x, t_floatarg f){
    x->x_ratio4 = f;
}

static void pm6_ratio5(t_pm6 *x, t_floatarg f){
    x->x_ratio5 = f;
}

static void pm6_ratio6(t_pm6 *x, t_floatarg f){
    x->x_ratio6 = f;
}

static void pm6_ratio(t_pm6 *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 6){
        pm6_ratio1(x, atom_getfloat(av));
        av++;
        pm6_ratio2(x, atom_getfloat(av));
        av++;
        pm6_ratio3(x, atom_getfloat(av));
        av++;
        pm6_ratio4(x, atom_getfloat(av));
        av++;
        pm6_ratio5(x, atom_getfloat(av));
        av++;
        pm6_ratio6(x, atom_getfloat(av));
    }
}

static void pm6_1to1(t_pm6 *x, t_floatarg f){
    x->x_1to1 = f;
}

static void pm6_1to2(t_pm6 *x, t_floatarg f){
    x->x_1to2 = f;
}

static void pm6_1to3(t_pm6 *x, t_floatarg f){
    x->x_1to3 = f;
}

static void pm6_1to4(t_pm6 *x, t_floatarg f){
    x->x_1to4 = f;
}

static void pm6_1to5(t_pm6 *x, t_floatarg f){
    x->x_1to5 = f;
}

static void pm6_1to6(t_pm6 *x, t_floatarg f){
    x->x_1to6 = f;
}

static void pm6_2to1(t_pm6 *x, t_floatarg f){
    x->x_2to1 = f;
}

static void pm6_2to2(t_pm6 *x, t_floatarg f){
    x->x_2to2 = f;
}

static void pm6_2to3(t_pm6 *x, t_floatarg f){
    x->x_2to3 = f;
}

static void pm6_2to4(t_pm6 *x, t_floatarg f){
    x->x_2to4 = f;
}

static void pm6_2to5(t_pm6 *x, t_floatarg f){
    x->x_2to5 = f;
}

static void pm6_2to6(t_pm6 *x, t_floatarg f){
    x->x_2to6 = f;
}

static void pm6_3to1(t_pm6 *x, t_floatarg f){
    x->x_3to1 = f;
}

static void pm6_3to2(t_pm6 *x, t_floatarg f){
    x->x_3to2 = f;
}

static void pm6_3to3(t_pm6 *x, t_floatarg f){
    x->x_3to3 = f;
}

static void pm6_3to4(t_pm6 *x, t_floatarg f){
    x->x_3to4 = f;
}

static void pm6_3to5(t_pm6 *x, t_floatarg f){
    x->x_3to5 = f;
}

static void pm6_3to6(t_pm6 *x, t_floatarg f){
    x->x_3to6 = f;
}

static void pm6_4to1(t_pm6 *x, t_floatarg f){
    x->x_4to1 = f;
}

static void pm6_4to2(t_pm6 *x, t_floatarg f){
    x->x_4to2 = f;
}

static void pm6_4to3(t_pm6 *x, t_floatarg f){
    x->x_4to3 = f;
}

static void pm6_4to4(t_pm6 *x, t_floatarg f){
    x->x_4to4 = f;
}

static void pm6_4to5(t_pm6 *x, t_floatarg f){
    x->x_4to5 = f;
}

static void pm6_4to6(t_pm6 *x, t_floatarg f){
    x->x_4to6 = f;
}

static void pm6_5to1(t_pm6 *x, t_floatarg f){
    x->x_5to1 = f;
}

static void pm6_5to2(t_pm6 *x, t_floatarg f){
    x->x_5to2 = f;
}

static void pm6_5to3(t_pm6 *x, t_floatarg f){
    x->x_5to3 = f;
}

static void pm6_5to4(t_pm6 *x, t_floatarg f){
    x->x_5to4 = f;
}

static void pm6_5to5(t_pm6 *x, t_floatarg f){
    x->x_5to5 = f;
}

static void pm6_5to6(t_pm6 *x, t_floatarg f){
    x->x_5to6 = f;
}

static void pm6_6to1(t_pm6 *x, t_floatarg f){
    x->x_6to1 = f;
}

static void pm6_6to2(t_pm6 *x, t_floatarg f){
    x->x_6to2 = f;
}

static void pm6_6to3(t_pm6 *x, t_floatarg f){
    x->x_6to3 = f;
}

static void pm6_6to4(t_pm6 *x, t_floatarg f){
    x->x_6to4 = f;
}

static void pm6_6to5(t_pm6 *x, t_floatarg f){
    x->x_6to5 = f;
}

static void pm6_6to6(t_pm6 *x, t_floatarg f){
    x->x_6to6 = f;
}

static void pm6_idx(t_pm6 *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 36){
        x->x_1to1 = atom_getfloat(av);
        av++;
        x->x_2to1 = atom_getfloat(av);
        av++;
        x->x_3to1 = atom_getfloat(av);
        av++;
        x->x_4to1 = atom_getfloat(av);
        av++;
        x->x_5to1 = atom_getfloat(av);
        av++;
        x->x_6to1 = atom_getfloat(av);
        av++;
        x->x_1to2 = atom_getfloat(av);
        av++;
        x->x_2to2 = atom_getfloat(av);
        av++;
        x->x_3to2 = atom_getfloat(av);
        av++;
        x->x_4to2 = atom_getfloat(av);
        av++;
        x->x_5to2 = atom_getfloat(av);
        av++;
        x->x_6to2 = atom_getfloat(av);
        av++;
        x->x_1to3 = atom_getfloat(av);
        av++;
        x->x_2to3 = atom_getfloat(av);
        av++;
        x->x_3to3 = atom_getfloat(av);
        av++;
        x->x_4to3 = atom_getfloat(av);
        av++;
        x->x_5to3 = atom_getfloat(av);
        av++;
        x->x_6to3 = atom_getfloat(av);
        av++;
        x->x_1to4 = atom_getfloat(av);
        av++;
        x->x_2to4 = atom_getfloat(av);
        av++;
        x->x_3to4 = atom_getfloat(av);
        av++;
        x->x_4to4 = atom_getfloat(av);
        av++;
        x->x_5to4 = atom_getfloat(av);
        av++;
        x->x_6to4 = atom_getfloat(av);
        av++;
        x->x_1to5 = atom_getfloat(av);
        av++;
        x->x_2to5 = atom_getfloat(av);
        av++;
        x->x_3to5 = atom_getfloat(av);
        av++;
        x->x_4to5 = atom_getfloat(av);
        av++;
        x->x_5to5 = atom_getfloat(av);
        av++;
        x->x_6to5 = atom_getfloat(av);
        av++;
        x->x_1to6 = atom_getfloat(av);
        av++;
        x->x_2to6 = atom_getfloat(av);
        av++;
        x->x_3to6 = atom_getfloat(av);
        av++;
        x->x_4to6 = atom_getfloat(av);
        av++;
        x->x_5to6 = atom_getfloat(av);
        av++;
        x->x_6to6 = atom_getfloat(av);
        av++;
    }
}

double pm6_wrap_phase(double phase){
    while(phase >= 1)
        phase -= 1.;
    while(phase < 0)
        phase += 1.;
    return(phase);
}

static t_int *pm6_perform(t_int *w){
    t_pm6 *x = (t_pm6 *)(w[1]);
    t_float *freq = (t_float *)(w[2]);
    t_float *l1 = (t_float *)(w[3]);
    t_float *l2 = (t_float *)(w[4]);
    t_float *l3 = (t_float *)(w[5]);
    t_float *l4 = (t_float *)(w[6]);
    t_float *l5 = (t_float *)(w[7]);
    t_float *l6 = (t_float *)(w[8]);
    t_float *out1 = (t_float *)(w[9]);
    t_float *out2 = (t_float *)(w[10]);
    int n = x->x_n;
    int ch2 = x->x_ch2, ch3 = x->x_ch3, ch4 = x->x_ch4;
    int ch5 = x->x_ch5, ch6 = x->x_ch6, ch7 = x->x_ch7;
    float *y1n1 = x->x_y1n1;
    float *y1n2 = x->x_y1n2;
    float *y2n1 = x->x_y2n1;
    float *y2n2 = x->x_y2n2;
    float *y3n1 = x->x_y3n1;
    float *y3n2 = x->x_y3n2;
    float *y4n1 = x->x_y4n1;
    float *y4n2 = x->x_y4n2;
    float *y5n1 = x->x_y5n1;
    float *y5n2 = x->x_y5n2;
    float *y6n1 = x->x_y6n1;
    float *y6n2 = x->x_y6n2;
    double *ph1 = x->x_phase_1;
    double *ph2 = x->x_phase_2;
    double *ph3 = x->x_phase_3;
    double *ph4 = x->x_phase_4;
    double *ph5 = x->x_phase_5;
    double *ph6 = x->x_phase_6;
    double pan1 = x->x_fpan1, pan2 = x->x_fpan2;
    double pan3 = x->x_fpan3, pan4 = x->x_fpan4;
    double pan5 = x->x_fpan5, pan6 = x->x_fpan6;
    double vol1 = x->x_fvol1, vol2 = x->x_fvol2;
    double vol3 = x->x_fvol3, vol4 = x->x_fvol4;
    double vol5 = x->x_fvol5, vol6 = x->x_fvol6;
    double p1Inc = (x->x_pan1 - pan1) * x->x_ramp;
    double p2Inc = (x->x_pan2 - pan2) * x->x_ramp;
    double p3Inc = (x->x_pan3 - pan3) * x->x_ramp;
    double p4Inc = (x->x_pan4 - pan4) * x->x_ramp;
    double p5Inc = (x->x_pan5 - pan5) * x->x_ramp;
    double p6Inc = (x->x_pan6 - pan6) * x->x_ramp;
    double v1Inc = (x->x_vol1 - vol1) * x->x_ramp;
    double v2Inc = (x->x_vol2 - vol2) * x->x_ramp;
    double v3Inc = (x->x_vol3 - vol3) * x->x_ramp;
    double v4Inc = (x->x_vol4 - vol4) * x->x_ramp;
    double v5Inc = (x->x_vol5 - vol5) * x->x_ramp;
    double v6Inc = (x->x_vol6 - vol6) * x->x_ramp;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double hz = freq[j*n + i];
            float level1 = ch2 == 1 ? l1[i] : l1[j*n + i];
            float level2 = ch3 == 1 ? l2[i] : l2[j*n + i];
            float level3 = ch4 == 1 ? l3[i] : l3[j*n + i];
            float level4 = ch5 == 1 ? l4[i] : l4[j*n + i];
            float level5 = ch6 == 1 ? l5[i] : l5[j*n + i];
            float level6 = ch7 == 1 ? l6[i] : l6[j*n + i];
            
            float mod1 = ((y1n1[j] + y1n2[j]) * 0.5); // fb bus1
            float op1 = read_sintab(pm6_wrap_phase(ph1[j] + mod1));
            float bus1 = op1 * x->x_1to1;
            
            float mod2 = ((y2n1[j] + y2n2[j]) * 0.5); // fb bus2
            mod2 += (op1 * x->x_1to2); // ff
            float op2 = read_sintab(pm6_wrap_phase(ph2[j] + mod2));
            bus1 += (op2 * x->x_2to1);
            float bus2 = (op2 * x->x_2to2);
            
            float mod3 = ((y3n1[j] + y3n2[j]) * 0.5); // fb bus3
            mod3 += (op1 * x->x_1to3);
            mod3 += (op2 * x->x_2to3);
            float op3 = read_sintab(pm6_wrap_phase(ph3[j] + mod3));;
            bus1 += (op3 * x->x_3to1);
            bus2 += (op3 * x->x_3to2);
            float bus3 = op3 * x->x_3to3;
            
            float mod4 = ((y4n1[j] + y4n2[j]) * 0.5); // fb bus4
            mod4 += (op1 * x->x_1to4);
            mod4 += (op2 * x->x_2to4);
            mod4 += (op3 * x->x_3to4);
            float op4 = read_sintab(pm6_wrap_phase(ph4[j] + mod4));
            bus1 += (op4 * x->x_4to1);
            bus2 += (op4 * x->x_4to2);
            bus3 += (op4 * x->x_4to3);
            float bus4 = (op4 * x->x_4to4);
            
            float mod5 = ((y5n1[j] + y5n2[j]) * 0.5); // fb bus5
            mod5 += (op1 * x->x_1to5);
            mod5 += (op2 * x->x_2to5);
            mod5 += (op3 * x->x_3to5);
            mod5 += (op4 * x->x_4to5);
            float op5 = read_sintab(pm6_wrap_phase(ph5[j] + mod5));
            bus1 += (op5 * x->x_5to1);
            bus2 += (op5 * x->x_5to2);
            bus3 += (op5 * x->x_5to3);
            bus4 += (op5 * x->x_5to4);
            float bus5 = (op5 * x->x_5to5);
            
            float mod6 = ((y6n1[j] + y6n2[j]) * 0.5); // fb bus6
            mod6 += (op1 * x->x_1to6);
            mod6 += (op2 * x->x_2to6);
            mod6 += (op3 * x->x_3to6);
            mod6 += (op4 * x->x_4to6);
            mod6 += (op5 * x->x_5to6);
            float op6 = read_sintab(pm6_wrap_phase(ph6[j] + mod6));
            bus1 += (op6 * x->x_6to1);
            bus2 += (op6 * x->x_6to2);
            bus3 += (op6 * x->x_6to3);
            bus4 += (op6 * x->x_6to4);
            bus5 += (op6 * x->x_6to5);
            float bus6 = (op6 * x->x_6to6);
            
            double inc1 = (hz + x->x_detune1) * x->x_ratio1 * x->x_sr_rec;
            double inc2 = (hz + x->x_detune2) * x->x_ratio2 * x->x_sr_rec;
            double inc3 = (hz + x->x_detune3) * x->x_ratio3 * x->x_sr_rec;
            double inc4 = (hz + x->x_detune4) * x->x_ratio4 * x->x_sr_rec;
            double inc5 = (hz + x->x_detune5) * x->x_ratio5 * x->x_sr_rec;
            double inc6 = (hz + x->x_detune6) * x->x_ratio6 * x->x_sr_rec;
            ph1[j] = pm6_wrap_phase(ph1[j] + inc1); // phase inc
            ph2[j] = pm6_wrap_phase(ph2[j] + inc2); // phase inc
            ph3[j] = pm6_wrap_phase(ph3[j] + inc3); // phase inc
            ph4[j] = pm6_wrap_phase(ph4[j] + inc4); // phase inc
            ph5[j] = pm6_wrap_phase(ph5[j] + inc5); // phase inc
            ph6[j] = pm6_wrap_phase(ph6[j] + inc6); // phase inc
            
            float g1 = op1 * vol1 * level1;
            float g2 = op2 * vol2 * level2;
            float g3 = op3 * vol3 * level3;
            float g4 = op4 * vol4 * level4;
            float g5 = op5 * vol5 * level5;
            float g6 = op6 * vol6 * level6;
            float panL = 0;
            panL += (g1 * read_sintab(pan1 + 0.25));
            panL += (g2 * read_sintab(pan2 + 0.25));
            panL += (g3 * read_sintab(pan3 + 0.25));
            panL += (g4 * read_sintab(pan4 + 0.25));
            panL += (g5 * read_sintab(pan5 + 0.25));
            panL += (g6 * read_sintab(pan6 + 0.25));
            float panR = 0;
            panR += (g1 * read_sintab(pan1));
            panR += (g2 * read_sintab(pan2));
            panR += (g3 * read_sintab(pan3));
            panR += (g4 * read_sintab(pan4));
            panR += (g5 * read_sintab(pan5));
            panR += (g6 * read_sintab(pan6));
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
            y5n2[j] = y5n1[j];
            y5n1[j] = bus5;
            y6n2[j] = y6n1[j];
            y6n1[j] = bus6;
            
            pan1 += p1Inc;
            pan2 += p2Inc;
            pan3 += p3Inc;
            pan4 += p4Inc;
            pan5 += p5Inc;
            pan6 += p6Inc;
            vol1 += v1Inc;
            vol2 += v2Inc;
            vol3 += v3Inc;
            vol4 += v4Inc;
            vol5 += v5Inc;
            vol6 += v6Inc;
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
    x->x_y5n1 = y5n1;
    x->x_y5n2 = y5n2;
    x->x_y6n1 = y6n1;
    x->x_y6n2 = y6n2;
    x->x_phase_1 = ph1;
    x->x_phase_2 = ph2;
    x->x_phase_3 = ph3;
    x->x_phase_4 = ph4;
    x->x_phase_5 = ph5;
    x->x_phase_6 = ph6;
    x->x_fpan1 = pan1;
    x->x_fpan2 = pan2;
    x->x_fpan3 = pan3;
    x->x_fpan4 = pan4;
    x->x_fpan5 = pan5;
    x->x_fpan6 = pan6;
    x->x_fvol1 = vol1;
    x->x_fvol2 = vol2;
    x->x_fvol3 = vol3;
    x->x_fvol4 = vol4;
    x->x_fvol5 = vol5;
    x->x_fvol6 = vol6;
    return(w+11);
}

static void pm6_dsp(t_pm6 *x, t_signal **sp){
    x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    x->x_ramp = 100.f*x->x_sr_rec;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans, ch4 = sp[3]->s_nchans;
    int ch5 = sp[4]->s_nchans, ch6 = sp[5]->s_nchans, ch7 = sp[6]->s_nchans;
    if((ch2 > 1 && ch2 != chs) || (ch3 > 1 && ch3 != chs)
    || (ch4 > 1 && ch4 != chs) || (ch5 > 1 && ch5 != chs)
    || (ch6 > 1 && ch6 != chs) || (ch7 > 1 && ch7 != chs)){
        signal_setmultiout(&sp[7], 1);
        signal_setmultiout(&sp[8], 1);
        dsp_add_zero(sp[7]->s_vec, x->x_n);
        dsp_add_zero(sp[8]->s_vec, x->x_n);
        pd_error(x, "[pm6~]: channel sizes mismatch");
        return;
    }
    signal_setmultiout(&sp[7], chs);
    signal_setmultiout(&sp[8], chs);
    x->x_ch2 = ch2, x->x_ch3 = ch3, x->x_ch4 = ch4;
    x->x_ch5 = ch5, x->x_ch6 = ch6, x->x_ch7 = ch7;
    if(x->x_nchans != chs){
        x->x_phase_1 = (double *)resizebytes(x->x_phase_1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase_2 = (double *)resizebytes(x->x_phase_2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase_3 = (double *)resizebytes(x->x_phase_3,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase_4 = (double *)resizebytes(x->x_phase_4,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase_5 = (double *)resizebytes(x->x_phase_5,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase_6 = (double *)resizebytes(x->x_phase_6,
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
        x->x_y5n1 = (float *)resizebytes(x->x_y5n1,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y5n2 = (float *)resizebytes(x->x_y5n2,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y6n1 = (float *)resizebytes(x->x_y6n1,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_y6n2 = (float *)resizebytes(x->x_y6n2,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_nchans = chs;
    }
    dsp_add(pm6_perform, 10, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec,
        sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec);
}

static void *pm6_free(t_pm6 *x){
    inlet_free(x->x_inl1);
    inlet_free(x->x_inl2);
    inlet_free(x->x_inl3);
    inlet_free(x->x_inl4);
    inlet_free(x->x_inl5);
    inlet_free(x->x_inl6);
    freebytes(x->x_phase_1, x->x_nchans * sizeof(*x->x_phase_1));
    freebytes(x->x_phase_2, x->x_nchans * sizeof(*x->x_phase_2));
    freebytes(x->x_phase_3, x->x_nchans * sizeof(*x->x_phase_3));
    freebytes(x->x_phase_4, x->x_nchans * sizeof(*x->x_phase_4));
    freebytes(x->x_phase_5, x->x_nchans * sizeof(*x->x_phase_5));
    freebytes(x->x_phase_6, x->x_nchans * sizeof(*x->x_phase_6));
    freebytes(x->x_y1n1, x->x_nchans * sizeof(*x->x_y1n1));
    freebytes(x->x_y1n2, x->x_nchans * sizeof(*x->x_y1n2));
    freebytes(x->x_y2n1, x->x_nchans * sizeof(*x->x_y2n1));
    freebytes(x->x_y2n2, x->x_nchans * sizeof(*x->x_y2n2));
    freebytes(x->x_y3n1, x->x_nchans * sizeof(*x->x_y3n1));
    freebytes(x->x_y3n2, x->x_nchans * sizeof(*x->x_y3n2));
    freebytes(x->x_y4n1, x->x_nchans * sizeof(*x->x_y4n1));
    freebytes(x->x_y4n2, x->x_nchans * sizeof(*x->x_y4n2));
    freebytes(x->x_y5n1, x->x_nchans * sizeof(*x->x_y5n1));
    freebytes(x->x_y5n2, x->x_nchans * sizeof(*x->x_y5n2));
    freebytes(x->x_y6n1, x->x_nchans * sizeof(*x->x_y6n1));
    freebytes(x->x_y6n2, x->x_nchans * sizeof(*x->x_y6n2));
    return(void *)x;
}

static void *pm6_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_pm6 *x = (t_pm6 *)pd_new(pm6_class);
    x->x_phase_1 = (double *)getbytes(sizeof(*x->x_phase_1));
    x->x_phase_2 = (double *)getbytes(sizeof(*x->x_phase_2));
    x->x_phase_3 = (double *)getbytes(sizeof(*x->x_phase_3));
    x->x_phase_4 = (double *)getbytes(sizeof(*x->x_phase_4));
    x->x_phase_5 = (double *)getbytes(sizeof(*x->x_phase_5));
    x->x_phase_6 = (double *)getbytes(sizeof(*x->x_phase_6));
    x->x_y1n1 = (float *)getbytes(sizeof(*x->x_y1n1));
    x->x_y1n2 = (float *)getbytes(sizeof(*x->x_y1n2));
    x->x_y2n1 = (float *)getbytes(sizeof(*x->x_y2n1));
    x->x_y2n2 = (float *)getbytes(sizeof(*x->x_y2n2));
    x->x_y3n1 = (float *)getbytes(sizeof(*x->x_y3n1));
    x->x_y3n2 = (float *)getbytes(sizeof(*x->x_y3n2));
    x->x_y4n1 = (float *)getbytes(sizeof(*x->x_y4n1));
    x->x_y4n2 = (float *)getbytes(sizeof(*x->x_y4n2));
    x->x_y5n1 = (float *)getbytes(sizeof(*x->x_y5n1));
    x->x_y5n2 = (float *)getbytes(sizeof(*x->x_y5n2));
    x->x_y6n1 = (float *)getbytes(sizeof(*x->x_y6n1));
    x->x_y6n2 = (float *)getbytes(sizeof(*x->x_y6n2));
    init_sine_table();
    x->x_ratio1 = x->x_ratio2 = x->x_ratio3 = x->x_ratio4 = x->x_ratio5 = x->x_ratio6 = 1;
    x->x_vol1 = x->x_fvol1 = x->x_fvol2 = x->x_vol2 = 1;
    x->x_vol3 = x->x_fvol3 = x->x_fvol4 = x->x_vol4 = 1;
    x->x_vol5 = x->x_fvol5 = x->x_fvol6 = x->x_vol6 = 1;
    x->x_pan1 = x->x_fpan1 = x->x_fpan2 = x->x_pan2 = .125;
    x->x_pan3 = x->x_fpan3 = x->x_fpan4 = x->x_pan4 = .125;
    x->x_pan5 = x->x_fpan5 = x->x_fpan6 = x->x_pan6 = .125;
    while(ac){
        if(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-ratio")){
                if(ac < 7)
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
                x->x_ratio5 = atom_getfloat(av);
                ac--, av++;
                x->x_ratio6 = atom_getfloat(av);
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-detune")){
                if(ac < 7)
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
                x->x_detune5 = atom_getfloat(av);
                ac--, av++;
                x->x_detune6 = atom_getfloat(av);
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-idx")){
                if(ac < 37)
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
                x->x_5to1 = atom_getfloat(av);
                ac--, av++;
                x->x_6to1 = atom_getfloat(av);
                ac--, av++;
                x->x_1to2 = atom_getfloat(av);
                ac--, av++;
                x->x_2to2 = atom_getfloat(av);
                ac--, av++;
                x->x_3to2 = atom_getfloat(av);
                ac--, av++;
                x->x_4to2 = atom_getfloat(av);
                ac--, av++;
                x->x_5to2 = atom_getfloat(av);
                ac--, av++;
                x->x_6to2 = atom_getfloat(av);
                ac--, av++;
                x->x_1to3 = atom_getfloat(av);
                ac--, av++;
                x->x_2to3 = atom_getfloat(av);
                ac--, av++;
                x->x_3to3 = atom_getfloat(av);
                ac--, av++;
                x->x_4to3 = atom_getfloat(av);
                ac--, av++;
                x->x_5to3 = atom_getfloat(av);
                ac--, av++;
                x->x_6to3 = atom_getfloat(av);
                ac--, av++;
                x->x_1to4 = atom_getfloat(av);
                ac--, av++;
                x->x_2to4 = atom_getfloat(av);
                ac--, av++;
                x->x_3to4 = atom_getfloat(av);
                ac--, av++;
                x->x_4to4 = atom_getfloat(av);
                ac--, av++;
                x->x_5to4 = atom_getfloat(av);
                ac--, av++;
                x->x_6to4 = atom_getfloat(av);
                ac--, av++;
                x->x_1to5 = atom_getfloat(av);
                ac--, av++;
                x->x_2to5 = atom_getfloat(av);
                ac--, av++;
                x->x_3to5 = atom_getfloat(av);
                ac--, av++;
                x->x_4to5 = atom_getfloat(av);
                ac--, av++;
                x->x_5to5 = atom_getfloat(av);
                ac--, av++;
                x->x_6to5 = atom_getfloat(av);
                ac--, av++;
                x->x_1to6 = atom_getfloat(av);
                ac--, av++;
                x->x_2to6 = atom_getfloat(av);
                ac--, av++;
                x->x_3to6 = atom_getfloat(av);
                ac--, av++;
                x->x_4to6 = atom_getfloat(av);
                ac--, av++;
                x->x_5to6 = atom_getfloat(av);
                ac--, av++;
                x->x_6to6 = atom_getfloat(av);
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-vol")){
                if(ac < 7)
                    goto errstate;
                ac--, av++;
                pm6_vol1(x, atom_getfloat(av));
                ac--, av++;
                pm6_vol2(x, atom_getfloat(av));
                ac--, av++;
                pm6_vol3(x, atom_getfloat(av));
                ac--, av++;
                pm6_vol4(x, atom_getfloat(av));
                ac--, av++;
                pm6_vol5(x, atom_getfloat(av));
                ac--, av++;
                pm6_vol6(x, atom_getfloat(av));
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-pan")){
                if(ac < 7)
                    goto errstate;
                ac--, av++;
                pm6_pan1(x, atom_getfloat(av));
                ac--, av++;
                pm6_pan2(x, atom_getfloat(av));
                ac--, av++;
                pm6_pan3(x, atom_getfloat(av));
                ac--, av++;
                pm6_pan4(x, atom_getfloat(av));
                ac--, av++;
                pm6_pan5(x, atom_getfloat(av));
                ac--, av++;
                pm6_pan6(x, atom_getfloat(av));
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
    x->x_inl5 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inl5, 1);
    x->x_inl6 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inl6, 1);
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
errstate:
    pd_error(x, "[pm6~]: improper args");
    return(NULL);
}

void pm6_tilde_setup(void){
    pm6_class = class_new(gensym("pm6~"), (t_newmethod)pm6_new, (t_method)pm6_free, sizeof(t_pm6), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(pm6_class, t_pm6, x_freq);
    class_addmethod(pm6_class, (t_method)pm6_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(pm6_class, (t_method)pm6_idx, gensym("idx"), A_GIMME, 0);
    class_addmethod(pm6_class, (t_method)pm6_1to1, gensym("1to1"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_1to2, gensym("1to2"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_1to3, gensym("1to3"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_1to4, gensym("1to4"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_1to5, gensym("1to5"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_1to6, gensym("1to6"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_2to1, gensym("2to1"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_2to2, gensym("2to2"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_2to3, gensym("2to3"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_2to4, gensym("2to4"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_2to5, gensym("2to5"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_2to6, gensym("2to6"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_3to1, gensym("3to1"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_3to2, gensym("3to2"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_3to3, gensym("3to3"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_3to4, gensym("3to4"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_3to5, gensym("3to5"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_3to6, gensym("3to6"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_4to1, gensym("4to1"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_4to2, gensym("4to2"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_4to3, gensym("4to3"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_4to4, gensym("4to4"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_4to5, gensym("4to5"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_4to6, gensym("4to6"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_5to1, gensym("5to1"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_5to2, gensym("5to2"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_5to3, gensym("5to3"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_5to4, gensym("5to4"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_5to5, gensym("5to5"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_5to6, gensym("5to6"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_6to1, gensym("6to1"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_6to2, gensym("6to2"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_6to3, gensym("6to3"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_6to4, gensym("6to4"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_6to5, gensym("6to5"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_6to6, gensym("6to6"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_ratio, gensym("ratio"), A_GIMME, 0);
    class_addmethod(pm6_class, (t_method)pm6_ratio1, gensym("ratio1"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_ratio2, gensym("ratio2"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_ratio3, gensym("ratio3"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_ratio4, gensym("ratio4"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_ratio5, gensym("ratio5"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_ratio6, gensym("ratio6"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_detune, gensym("detune"), A_GIMME, 0);
    class_addmethod(pm6_class, (t_method)pm6_detune1, gensym("detune1"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_detune2, gensym("detune2"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_detune3, gensym("detune3"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_detune4, gensym("detune4"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_detune5, gensym("detune5"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_detune6, gensym("detune6"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_vol, gensym("vol"), A_GIMME, 0);
    class_addmethod(pm6_class, (t_method)pm6_vol1, gensym("vol1"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_vol2, gensym("vol2"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_vol3, gensym("vol3"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_vol4, gensym("vol4"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_vol5, gensym("vol5"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_vol6, gensym("vol6"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_pan, gensym("pan"), A_GIMME, 0);
    class_addmethod(pm6_class, (t_method)pm6_pan1, gensym("pan1"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_pan2, gensym("pan2"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_pan3, gensym("pan3"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_pan4, gensym("pan4"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_pan5, gensym("pan5"), A_FLOAT, 0);
    class_addmethod(pm6_class, (t_method)pm6_pan6, gensym("pan6"), A_FLOAT, 0);
}
