// Porres 2016 - 2025

#include <m_pd.h>

static t_class *sh_class;

typedef struct _sh{
    t_object    x_obj;
    t_inlet    *x_trig_inlet;
    t_float     x_f;
    t_float    *x_lastout;
    t_float    *x_last_trig;
    t_float     x_trig_bang;
    t_float     x_thresh;
    t_int       x_mode;
    t_int       x_n;
    t_int       x_ch1;
    t_int       x_ch2;
    t_int       x_nchs;
    t_symbol   *x_ignore;
}t_sh;

static void sh_thresh(t_sh *x, t_floatarg f){
    x->x_thresh = f;
}

static void sh_gate(t_sh *x){
    x->x_mode = 0;
}

static void sh_trigger(t_sh *x){
    x->x_mode = 1;
}

static void sh_set(t_sh *x, t_floatarg f){
    for(int i = 0; i < x->x_nchs; i++)
        x->x_lastout[i] = f;
}

static void sh_bang(t_sh *x){
    x->x_trig_bang = 1;
}

static t_int *sh_perform(t_int *w){
    t_sh *x = (t_sh *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    t_float *output = x->x_lastout;
    t_float *last_trig = x->x_last_trig;
    for(int j = 0; j < x->x_nchs; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            float input;
            if(x->x_ch1 == 1)
                input = in1[i];
            else
                input = in1[j*n + i];
            float trigger;
            if(x->x_ch2 == 1)
                trigger = in2[i];
            else
                trigger = in2[j*n + i];
            if(x->x_trig_bang){
                output[j] = input;
                x->x_trig_bang = 0;
            }
            if(!x->x_mode && trigger > x->x_thresh)
                output[j] = input;
            if(x->x_mode && trigger > x->x_thresh && last_trig[j] <= x->x_thresh)
                output[j] = input;
            out[j*n + i] = output[j];
            last_trig[j] = trigger;
        }
    }
    x->x_lastout = output;
    x->x_last_trig = last_trig;
    return(w+5);
}

static void sh_dsp(t_sh *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = x->x_ch1 = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    if(x->x_ch2 > chs)
        chs = x->x_ch2;
    if(x->x_nchs != chs){
        x->x_lastout = (t_float *)resizebytes(x->x_lastout,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_last_trig = (t_float *)resizebytes(x->x_last_trig,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchs = chs;
    }
    signal_setmultiout(&sp[2], x->x_nchs);
    if(x->x_ch1 > 1 && x->x_ch1 != x->x_nchs ||
       x->x_ch2 > 1 && x->x_ch2 != x->x_nchs){
            dsp_add_zero(sp[2]->s_vec, x->x_nchs*x->x_n);
            pd_error(x, "[sh~]: channel sizes mismatch");
            return;
    }
    dsp_add(sh_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *sh_free(t_sh *x){
    inlet_free(x->x_trig_inlet);
    freebytes(x->x_lastout, x->x_nchs * sizeof(*x->x_lastout));
    freebytes(x->x_last_trig, x->x_nchs * sizeof(*x->x_last_trig));
    return(void *)x;
}

static void *sh_new(t_symbol *s, int argc, t_atom *argv){
    t_sh *x = (t_sh *)pd_new(sh_class);
    x->x_ignore = s;
    float init_thresh = 0, init_value = 0;
    int init_mode = 0;
    x->x_lastout = (t_float *)getbytes(sizeof(t_float));
    x->x_last_trig = (t_float *)getbytes(sizeof(t_float));
/////////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(argc > 0){
        if(argv -> a_type == A_FLOAT){ //if current argument is a float
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum){
                case 0:
                    init_thresh = argval;
                    break;
                case 1:
                    init_value = argval;
                    break;
                case 2:
                    init_mode = argval != 0;
                    break;
                default:
                    break;
            };
            argnum++;
            argc--, argv++;
        }
        else if(argv->a_type == A_SYMBOL && !argnum){
            if(atom_getsymbolarg(0, argc, argv) == gensym("-tr")){
                init_mode = 1;
                argc--, argv++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
/////////////////////////////////////////////////////////////////////////////////////
    x->x_trig_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    x->x_thresh = init_thresh;
    x->x_lastout[0] = init_value;
    x->x_mode = init_mode;
    return(x);
    errstate:
        pd_error(x, "[sh~]: improper args");
        return NULL;
}

void sh_tilde_setup(void){
    sh_class = class_new(gensym("sh~"), (t_newmethod)(void *)sh_new,
        (t_method)sh_free, sizeof(t_sh), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(sh_class, nullfn, gensym("signal"), 0);
    CLASS_MAINSIGNALIN(sh_class, t_sh, x_f);
    class_addbang(sh_class,(t_method)sh_bang);
    class_addmethod(sh_class, (t_method)sh_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(sh_class, (t_method)sh_set, gensym("set"), A_DEFFLOAT, 0);
    class_addmethod(sh_class, (t_method)sh_thresh, gensym("thresh"), A_DEFFLOAT, 0);
    class_addmethod(sh_class, (t_method)sh_trigger, gensym("trigger"), 0);
    class_addmethod(sh_class, (t_method)sh_gate, gensym("gate"), 0);
}
