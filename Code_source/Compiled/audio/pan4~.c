// Porres 2016

#include "m_pd.h"
#include <math.h>

#define HALF_PI (3.14159265358979323846 * 0.5)

static t_class *pan4_class;

typedef struct _pan4{
    t_object    x_obj;
    t_int       x_n;
    t_int       x_mc;
    t_inlet    *x_xpan_inlet;
    t_inlet    *x_ypan_inlet;
}t_pan4;

static t_int *pan4_perform(t_int *w){
    t_pan4 *x = (t_pan4 *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);  // in
    t_float *in2 = (t_float *)(w[3]);  // xpan
    t_float *in3 = (t_float *)(w[4]);  // ypan
    t_float *out1 = (t_float *)(w[5]); // BL
    t_float *out2 = (t_float *)(w[6]); // FL
    t_float *out3 = (t_float *)(w[7]); // BL
    t_float *out4 = (t_float *)(w[8]); // FL
    int n = x->x_n;
    while(n--){
        float in = *in1++;
        float xpan = (*in2++ + 1) * 0.5;
        float ypan = (*in3++ + 1) * 0.5;
        if(xpan < 0)
            xpan = 0;
        if(xpan > 1)
            xpan = 1;
        if(ypan < 0)
            ypan = 0;
        if(ypan > 1)
            ypan = 1;
        float back = in * (ypan == 1 ? 0 : cos(ypan * HALF_PI));
        float front = in * sin(ypan * HALF_PI);
        *out1++ = back * (xpan == 1 ? 0 : cos(xpan * HALF_PI));  // Left Back
        *out2++ = front * (xpan == 1 ? 0 : cos(xpan * HALF_PI)); // Left Front
        *out3++ = front * sin(xpan * HALF_PI);                   // Right Front
        *out4++ = back * sin(xpan * HALF_PI);                    // Right Back
    }
    return(w+9);
}

static t_int *pan4_perform_mc(t_int *w){
    t_pan4 *x = (t_pan4 *)(w[1]);
    t_float *in1 = (t_float *)(w[2]); // in
    t_float *in2 = (t_float *)(w[3]); // xpan
    t_float *in3 = (t_float *)(w[4]); // ypan
    t_float *out = (t_float *)(w[5]);
    for(int i = 0; i < x->x_n; i++){
        float in = in1[i];
        float xpan = (in2[i] + 1) * 0.5;
        float ypan = (in3[i] + 1) * 0.5;
        if(xpan < 0)
            xpan = 0;
        if(xpan > 1)
            xpan = 1;
        if(ypan < 0)
            ypan = 0;
        if(ypan > 1)
            ypan = 1;
        float back = in * (ypan == 1 ? 0 : cos(ypan * HALF_PI));
        float front = in * sin(ypan * HALF_PI);
        for(int j = 0; j < 4; j++){
            float output = 0;
            if(j == 0)
                output = back * (xpan == 1 ? 0 : cos(xpan * HALF_PI));  // Left Back
            else if(j == 1)
                output = front * (xpan == 1 ? 0 : cos(xpan * HALF_PI)); // Left Front
            else if(j == 2)
                output = front * sin(xpan * HALF_PI);                   // Right Front
            else
                output = back * sin(xpan * HALF_PI);                    // Right Back8
            out[j*x->x_n + i] = output;
        }
    }
    return(w+6);
}

static void pan4_dsp(t_pan4 *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    if(!x->x_mc){
        signal_setmultiout(&sp[3], 1);
        signal_setmultiout(&sp[4], 1);
        signal_setmultiout(&sp[5], 1);
        signal_setmultiout(&sp[6], 1);
        dsp_add(pan4_perform, 8, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
            sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec);
    }
    else{
        signal_setmultiout(&sp[3], 4);
        dsp_add(pan4_perform_mc, 5, x, sp[0]->s_vec, sp[1]->s_vec,
            sp[2]->s_vec, sp[3]->s_vec);
    }
}

static void *pan4_free(t_pan4 *x){
    inlet_free(x->x_xpan_inlet);
    inlet_free(x->x_ypan_inlet);
    return (void *)x;
}

static void *pan4_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_pan4 *x = (t_pan4 *)pd_new(pan4_class);
    float f1 = 0, f2 = 0;
    x->x_mc = 0;
    while(ac && av->a_type == A_SYMBOL){
        if(atom_getsymbol(av) == gensym("-mc")){
            x->x_mc = 1;
            ac--, av++;
        }
    }
    if(ac && av->a_type == A_FLOAT){
        f1 = atom_getfloat(av);
        ac--, av++;
    }
    if(ac && av->a_type == A_FLOAT){
        f2 = atom_getfloat(av);
        ac--, av++;
    }
    x->x_xpan_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_xpan_inlet, f1);
    x->x_ypan_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_ypan_inlet, f2);
    outlet_new((t_object *)x, &s_signal);
    if(!x->x_mc)
        outlet_new((t_object *)x, &s_signal);
    if(!x->x_mc)
        outlet_new((t_object *)x, &s_signal);
    if(!x->x_mc)
        outlet_new((t_object *)x, &s_signal);
    return(x);
}

void pan4_tilde_setup(void){
    pan4_class = class_new(gensym("pan4~"), (t_newmethod)pan4_new,
        (t_method)pan4_free, sizeof(t_pan4), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(pan4_class, nullfn, gensym("signal"), 0);
    class_addmethod(pan4_class, (t_method)pan4_dsp, gensym("dsp"), A_CANT, 0);
}

