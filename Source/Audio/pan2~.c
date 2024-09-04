// Porres 2016

#include <m_pd.h>
#include <buffer.h>

static t_class *pan2_class;

typedef struct _pan2{
    t_object    x_obj;
    t_int       x_n;
    t_int       x_mc;
    t_inlet    *x_panlet;
}t_pan2;

static t_int *pan2_perform(t_int *w){
    t_pan2 *x = (t_pan2 *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);   // in
    t_float *in2 = (t_float *)(w[3]);   // pan
    t_float *out1 = (t_float *)(w[4]);  // L
    t_float *out2 = (t_float *)(w[5]);  // R
    int n = x->x_n;
    while(n--){
        float in = *in1++;
        float pan = (*in2++ + 1) * 0.125;
        if(pan < 0)
            pan = 0;
        if(pan > 0.25)
            pan = 0.25;
        *out1++ = in * read_sintab(pan + 0.25);
        *out2++ = in * read_sintab(pan);
    }
    return(w+6);
}

static t_int *pan2_perform_mc(t_int *w){
    t_pan2 *x = (t_pan2 *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);   // in
    t_float *in2 = (t_float *)(w[3]);   // pan
    t_float *out = (t_float *)(w[4]);
    for(int i = 0; i < x->x_n; i++){
        float in = in1[i];
        float pan = (in2[i] + 1) * 0.125;
        if(pan < 0)
            pan = 0;
        if(pan > 1)
            pan = 1;
        for(int j = 0; j < 2; j++){
            float output;
            if(j == 0)
                output = in * read_sintab(pan + 0.25);
            else
                output = in * read_sintab(pan);
            out[j*x->x_n + i] = output;
        }
    }
    return(w+5);
}

static void pan2_dsp(t_pan2 *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    if(!x->x_mc){
        signal_setmultiout(&sp[2], 1);
        signal_setmultiout(&sp[3], 1);
        dsp_add(pan2_perform, 5, x, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
    }
    else{
        signal_setmultiout(&sp[2], 2);
        dsp_add(pan2_perform_mc, 4, x, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec);
    }
}

static void *pan2_free(t_pan2 *x){
    inlet_free(x->x_panlet);
    return(void *)x;
}

static void *pan2_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_pan2 *x = (t_pan2 *)pd_new(pan2_class);
    init_sine_table();
    float f = 0;
    x->x_mc = 0;
    while(ac && av->a_type == A_SYMBOL){
        if(atom_getsymbol(av) == gensym("-mc")){
            x->x_mc = 1;
            ac--, av++;
        }
    }
    if(ac && av->a_type == A_FLOAT)
        f = atom_getfloat(av);
    x->x_panlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_panlet, f);
    outlet_new((t_object *)x, &s_signal);
    if(!x->x_mc)
        outlet_new((t_object *)x, &s_signal);
    return(x);
}

void pan2_tilde_setup(void){
    pan2_class = class_new(gensym("pan2~"), (t_newmethod)pan2_new,
        (t_method)pan2_free, sizeof(t_pan2), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(pan2_class, nullfn, gensym("signal"), 0);
    class_addmethod(pan2_class, (t_method)pan2_dsp, gensym("dsp"), A_CANT, 0);
}
