// Porres 2016-2025

#include <m_pd.h>
#include <stdlib.h>

static t_class *detect_class;

typedef struct _detect{
    t_object  x_obj;
    t_float  *x_count;
    t_float  *x_total;
    t_float  *x_lastin;
    t_float   x_sr;
    t_int     x_mode;
    t_int     x_nchs;
    t_int     x_n;
    t_int     x_active;
    t_outlet *x_outlet;
}t_detect;

static t_int *detect_perform(t_int *w){
    t_detect *x = (t_detect *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    t_float *lastin = x->x_lastin;
    t_float *count = x->x_count;
    t_float *total = x->x_total;
    int n = x->x_n;
    for(int j = 0; j < x->x_nchs; j++){
        for(int i = 0; i < n; i++){
            t_float input = in[j*n+i];
            if(input > 0 && lastin[j] <= 0){
                if(x->x_active)
                    total[j] = count[j];
                else
                    x->x_active = 1;
                count[j] = 1;
            }
            else
                count[j]++;
            t_float output = total[j];
            if(x->x_mode == 1)
                output = output * 1000 / x->x_sr;
            else if(x->x_mode == 2)
                output = x->x_sr / output;
            else if(x->x_mode == 3 && output > 0)
                output = 60 * x->x_sr / output;
            out[j*n+i] = output;
            lastin[j] = input;
        }
    }
    x->x_count = count;
    x->x_total = total;
    x->x_lastin = lastin;
    return(w+4);
}

static void detect_dsp(t_detect *x, t_signal **sp){
    x->x_sr = (t_float)sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(x->x_nchs != chs){
        x->x_lastin = (t_float *)resizebytes(x->x_lastin,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_count = (t_float *)resizebytes(x->x_count,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_total = (t_float *)resizebytes(x->x_total,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchs = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchs);
    dsp_add(detect_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void *detect_free(t_detect *x){
    outlet_free(x->x_outlet);
    freebytes(x->x_lastin, x->x_nchs * sizeof(*x->x_lastin));
    freebytes(x->x_count, x->x_nchs * sizeof(*x->x_count));
    freebytes(x->x_total, x->x_nchs * sizeof(*x->x_total));
    return(void *)x;
}

static void *detect_new(t_symbol *s, int ac, t_atom *av){
    t_detect *x = (t_detect *)pd_new(detect_class);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_mode = x->x_active = 0;
    x->x_sr = sys_getsr();
    x->x_nchs = 1;
    x->x_count = (t_float*)malloc(sizeof(t_float));
    x->x_count[0] = 0;
    x->x_total = (t_float*)malloc(sizeof(t_float));
    x->x_total[0] = 0;
    x->x_lastin = (t_float*)malloc(sizeof(t_float));
    x->x_lastin[0] = 0;
    if(ac == 1){
        if(av->a_type == A_SYMBOL){
            t_symbol *curarg = s; // get rid of warning
            curarg = atom_getsymbol(av);
            if(curarg == gensym("samps"))
                x->x_mode = 0;
            else if(curarg == gensym("ms"))
                x->x_mode = 1;
            else if(curarg == gensym("hz"))
                x->x_mode = 2;
            else if(curarg == gensym("bpm"))
                x->x_mode = 3;
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    else if(ac > 1)
        goto errstate;
    return(x);
errstate:
    pd_error(x, "[detect~]: improper args");
    return NULL;
}

void detect_tilde_setup(void){
    detect_class = class_new(gensym("detect~"), (t_newmethod)(void*)detect_new,
        (t_method)detect_free, sizeof(t_detect), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(detect_class, nullfn, gensym("signal"), 0);
    class_addmethod(detect_class, (t_method) detect_dsp, gensym("dsp"), A_CANT, 0);
}
