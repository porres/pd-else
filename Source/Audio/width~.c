// Porres 2016

#include <m_pd.h>
#include <math.h>
#include "buffer.h"
#include "magic.h"

#include <stdlib.h>

#define MAXLEN 1024

static t_class *width_class;

typedef struct _width{
    t_object    x_obj;
    int         x_nblock;
    int         x_nchans;
    t_int       x_sig3;
    t_int       x_sig4;
    float      *x_width_list;
    t_inlet    *x_widthlet;
    t_int       x_widthlist_size;
    t_symbol   *x_ignore;
    t_glist    *x_glist;
    t_float    *x_signalscalar;
}t_width;

static void width_width(t_width *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_widthlist_size != ac){
        x->x_widthlist_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_width_list[i] = atom_getfloat(av+i);
}

static t_int *width_perform(t_int *w){
    t_width *x = (t_width *)(w[1]);
    int ch3 = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);   // left
    t_float *in2 = (t_float *)(w[4]);   // right
    t_float *in3 = (t_float *)(w[5]);   // width
    t_float *out1 = (t_float *)(w[6]);  // L
    t_float *out2 = (t_float *)(w[7]);  // R
    if(!x->x_sig3){
        t_float *scalar = x->x_signalscalar;
        if(!else_magic_isnan(*x->x_signalscalar)){
            t_float width = *scalar;
            for(int j = 0; j < x->x_nchans; j++)
                x->x_width_list[j] = width;
            else_magic_setnan(x->x_signalscalar);
        }
    }
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < x->x_nblock; i++){
            float inL = in1[j*x->x_nblock + i];
            float inR = in2[j*x->x_nblock + i];
            int ch = j * x->x_nblock, mc = ch3 > 1;
            float width = x->x_sig3 ? in3[i+ch*mc] : x->x_width_list[j*mc];
            float mid = (inL + inR) * 0.5f;
            float side = (inL - inR) * 0.5f;
            side *= width;
            out1[j*x->x_nblock + i] = mid+side;
            out2[j*x->x_nblock + i] = mid-side;
        }
    }
    return(w+8);
}

static void width_dsp(t_width *x, t_signal **sp){
    x->x_nblock = sp[0]->s_n;
    x->x_nchans = sp[0]->s_nchans;
    int ch2 = sp[1]->s_nchans;
    x->x_sig3 = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal);
    int ch3 = x->x_sig3 ? sp[2]->s_nchans : x->x_widthlist_size;
    signal_setmultiout(&sp[3], x->x_nchans);
    signal_setmultiout(&sp[4], x->x_nchans);
    if((ch2 != x->x_nchans)
    || (ch3 > 1 && ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_nblock);
        dsp_add_zero(sp[4]->s_vec, x->x_nchans*x->x_nblock);
        pd_error(x, "[width~]: channel sizes mismatch");
    }
    dsp_add(width_perform, 7, x, ch3, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void *width_free(t_width *x){
    inlet_free(x->x_widthlet);
    free(x->x_width_list);
    return(void *)x;
}

static void *width_new(t_symbol *s, int ac, t_atom *av){
    t_width *x = (t_width *)pd_new(width_class);
    x->x_ignore = s;
    init_sine_table();
    x->x_width_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_width_list[0] = 0;
    x->x_widthlist_size = 1;
    float w = 1;
    if(ac && av->a_type == A_FLOAT){
        w = x->x_width_list[0] = atom_getfloat(av);
        ac--, av++;
    }
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_widthlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_widthlet, w);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    x->x_glist = canvas_getcurrent();
    x->x_signalscalar = obj_findsignalscalar((t_object *)x, 2);
    return(x);
}

void width_tilde_setup(void){
    width_class = class_new(gensym("width~"), (t_newmethod)width_new,
        (t_method)width_free, sizeof(t_width), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(width_class, nullfn, gensym("signal"), 0);
    class_addmethod(width_class, (t_method)width_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(width_class, (t_method)width_width, gensym("width"), A_GIMME, 0);
}
