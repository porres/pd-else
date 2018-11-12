// Porres 2016-2018

#include "m_pd.h"
#include <string.h>

static t_class *elapsed_class;

typedef struct _elapsed{
    t_object  x_obj;
    t_float   x_count;
    t_float   x_total;
    t_float   x_lastin;
    t_float   x_sr_khz;
    t_int     x_ms;
    t_outlet *x_outlet;
}t_elapsed;

static t_int *elapsed_perform(t_int *w){
    t_elapsed *x = (t_elapsed *)(w[1]);
    int n = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    t_float lastin = x->x_lastin;
    t_float count = x->x_count;
    t_float total = x->x_total;
    while(n--){
        t_float input = *in++;
        if(input > 0 && lastin <= 0){
            total = count;
            count = 1;
        }
        else
            count++;
        t_float output = total;
        if(x->x_ms)
            output /= x->x_sr_khz;
        *out++ = output;
        lastin = input;
    }
    x->x_count = count;
    x->x_total = total;
    x->x_lastin = lastin;
    return(w + 5);
}

static void elapsed_dsp(t_elapsed *x, t_signal **sp){
    x->x_sr_khz = (t_float)sp[0]->s_sr * 0.001;
    dsp_add(elapsed_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void *elapsed_free(t_elapsed *x){
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *elapsed_new(t_symbol *s, int ac, t_atom *av){
    t_elapsed *x = (t_elapsed *)pd_new(elapsed_class);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_count = x->x_total = x->x_lastin = x->x_ms = 0;
    x->x_sr_khz = sys_getsr() * 0.001;
    if(av->a_type == A_SYMBOL){
        t_symbol *curarg = s; // get rid of warning
        curarg = atom_getsymbolarg(0, ac, av);
        if(!strcmp(curarg->s_name, "-ms"))
            x->x_ms = 1;
        else
            goto errstate;
    }
    return(x);
errstate:
    pd_error(x, "elapsed~: improper args");
    return NULL;
}

void elapsed_tilde_setup(void){
    elapsed_class = class_new(gensym("elapsed~"), (t_newmethod)elapsed_new,
        (t_method)elapsed_free, sizeof(t_elapsed), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(elapsed_class, nullfn, gensym("signal"), 0);
    class_addmethod(elapsed_class, (t_method) elapsed_dsp, gensym("dsp"), A_CANT, 0);
}
