// Porres 2016

#include <m_pd.h>
#include "buffer.h"
#include "magic.h"

#include <stdlib.h>

#define MAXLEN 1024

static t_class *pan2_class;

typedef struct _pan2{
    t_object    x_obj;
    int         x_nblock;
    int         x_nchans;
    t_int       x_sig2;
    float      *x_pan_list;
    t_inlet    *x_panlet;
    t_int       x_list_size;
    t_symbol   *x_ignore;
    t_glist    *x_glist;
    t_float    *x_signalscalar;
}t_pan2;

static void pan2_pan(t_pan2 *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_list_size != ac){
        x->x_list_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_pan_list[i] = atom_getfloat(av+i);
}

static t_int *pan2_perform(t_int *w){
    t_pan2 *x = (t_pan2 *)(w[1]);
    int ch2 = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);   // in
    t_float *in2 = (t_float *)(w[4]);   // pan
    t_float *out1 = (t_float *)(w[5]);  // L
    t_float *out2 = (t_float *)(w[6]);  // R
    if(!x->x_sig2){
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
            float in = in1[j*x->x_nblock + i];
            float pan = x->x_sig2 ? ch2 == 1 ? in2[i] : in2[j*x->x_nblock + i] : x->x_pan_list[j];
            pan = (pan < -1 ? -1 : (pan > 1 ? 1 : ((pan + 1) * 0.125)));
            out1[j*x->x_nblock + i] = in * read_sintab(pan + 0.25);
            out2[j*x->x_nblock + i] = in * read_sintab(pan);
        }
    }
    return(w+7);
}

static void pan2_dsp(t_pan2 *x, t_signal **sp){
    x->x_nblock = sp[0]->s_n;
    x->x_nchans = sp[0]->s_nchans;
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    int ch2 = x->x_sig2 ? sp[1]->s_nchans : x->x_list_size;
    signal_setmultiout(&sp[2], x->x_nchans);
    signal_setmultiout(&sp[3], x->x_nchans);
    if((ch2 > 1 && ch2 != x->x_nchans)){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans*x->x_nblock);
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_nblock);
        pd_error(x, "[pan2~]: channel sizes mismatch");
    }
    dsp_add(pan2_perform, 6, x, ch2, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *pan2_free(t_pan2 *x){
    inlet_free(x->x_panlet);
    free(x->x_pan_list);
    return(void *)x;
}

static void *pan2_new(t_symbol *s, int ac, t_atom *av){
    t_pan2 *x = (t_pan2 *)pd_new(pan2_class);
    x->x_ignore = s;
    init_sine_table();
    x->x_pan_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_pan_list[0] = 0;
    x->x_list_size = 1;
    float f = 0;
    if(ac && av->a_type == A_FLOAT)
        f = x->x_pan_list[0] = atom_getfloat(av);
    x->x_panlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_panlet, f);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    x->x_glist = canvas_getcurrent();
    x->x_signalscalar = obj_findsignalscalar((t_object *)x, 1);
    return(x);
}

void pan2_tilde_setup(void){
    pan2_class = class_new(gensym("pan2~"), (t_newmethod)pan2_new,
        (t_method)pan2_free, sizeof(t_pan2), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(pan2_class, nullfn, gensym("signal"), 0);
    class_addmethod(pan2_class, (t_method)pan2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(pan2_class, (t_method)pan2_pan, gensym("pan"), A_GIMME, 0);
}
