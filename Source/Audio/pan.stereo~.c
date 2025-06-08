// Porres 2025

#include <m_pd.h>
#include <math.h>
#include "buffer.h"
#include "magic.h"

#include <stdlib.h>

#define MAXLEN 1024

static t_class *panstereo_class;

typedef struct _panstereo{
    t_object    x_obj;
    int         x_nblock;
    int         x_nchans;
    t_int       x_sig3;
    t_int       x_sig4;
    float      *x_pan_list;
    t_inlet    *x_panlet;
    t_int       x_panlist_size;
    t_symbol   *x_ignore;
    t_glist    *x_glist;
    t_float    *x_signalscalar;
}t_panstereo;

static void panstereo_pan(t_panstereo *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_panlist_size != ac){
        x->x_panlist_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_pan_list[i] = atom_getfloat(av+i);
}

static t_int *panstereo_perform(t_int *w){
    t_panstereo *x = (t_panstereo *)(w[1]);
    int ch3 = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);   // left
    t_float *in2 = (t_float *)(w[4]);   // right
    t_float *in3 = (t_float *)(w[5]);   // pan
    t_float *out1 = (t_float *)(w[6]);  // L
    t_float *out2 = (t_float *)(w[7]);  // R
    if(!x->x_sig3){
        t_float *scalar = x->x_signalscalar;
        if(!else_magic_isnan(*x->x_signalscalar)){
            t_float pan = *scalar;
            for(int j = 0; j < x->x_nchans; j++)
                x->x_pan_list[j] = pan;
            else_magic_setnan(x->x_signalscalar);
        }
    }
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < x->x_nblock; i++){
            float inL = in1[j*x->x_nblock + i];
            float inR = in2[j*x->x_nblock + i];
            int ch = j * x->x_nblock, mc = ch3 > 1;
            float pan = x->x_sig3 ? in3[i+ch*mc] : x->x_pan_list[j*mc];
            pan = (pan < -1 ? -1 : (pan > 1 ? 1 : pan));
            float gainL = 1, gainR = 1, LpanL = 1, LpanR = 1, RpanL = 1, RpanR = 1;
            if(pan < 0){
                gainL = 1./((pan * -1) + 1);
                LpanR = 0;
                RpanR = pan+1;
                RpanL = 1 - RpanR;
                RpanL = read_pantab(RpanL);
                RpanR = read_pantab(RpanR);
            }
            else{
                gainR = 1./(pan + 1);
                RpanL = 0;
                LpanL = 1 - pan;
                LpanR = 1 - LpanL;
                LpanL = read_pantab(LpanL);
                LpanR = read_pantab(LpanR);
            }
            out1[j*x->x_nblock + i] = (inL * LpanL + inR * RpanL) * gainL;
            out2[j*x->x_nblock + i] = (inL * LpanR + inR * RpanR) * gainR;
        }
    }
    return(w+8);
}

static void panstereo_dsp(t_panstereo *x, t_signal **sp){
    x->x_nblock = sp[0]->s_n;
    x->x_nchans = sp[0]->s_nchans;
    int ch2 = sp[1]->s_nchans;
    x->x_sig3 = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal);
    int ch3 = x->x_sig3 ? sp[2]->s_nchans : x->x_panlist_size;
    signal_setmultiout(&sp[3], x->x_nchans);
    signal_setmultiout(&sp[4], x->x_nchans);
    if((ch2 != x->x_nchans)
    || (ch3 > 1 && ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_nblock);
        dsp_add_zero(sp[4]->s_vec, x->x_nchans*x->x_nblock);
        pd_error(x, "[pan.stereo~]: channel sizes mismatch");
    }
    dsp_add(panstereo_perform, 7, x, ch3, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void *panstereo_free(t_panstereo *x){
    inlet_free(x->x_panlet);
    free(x->x_pan_list);
    return(void *)x;
}

static void *panstereo_new(t_symbol *s, int ac, t_atom *av){
    t_panstereo *x = (t_panstereo *)pd_new(panstereo_class);
    x->x_ignore = s;
    init_fade_tables();
    x->x_pan_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_pan_list[0] = 0;
    x->x_panlist_size = 1;
    float p = 0;
    if(ac && av->a_type == A_FLOAT){
        p = x->x_pan_list[0] = atom_getfloat(av);
        ac--, av++;
    }
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_panlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_panlet, p);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    x->x_glist = canvas_getcurrent();
    x->x_signalscalar = obj_findsignalscalar((t_object *)x, 2);
    return(x);
}

void setup_pan0x2estereo_tilde(void){
    panstereo_class = class_new(gensym("pan.stereo~"), (t_newmethod)panstereo_new,
        (t_method)panstereo_free, sizeof(t_panstereo), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(panstereo_class, nullfn, gensym("signal"), 0);
    class_addmethod(panstereo_class, (t_method)panstereo_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(panstereo_class, (t_method)panstereo_pan, gensym("pan"), A_GIMME, 0);
}
