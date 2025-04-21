// porres 2017-2024

#include <m_pd.h>
#include <buffer.h>

#define MAX_INOUT 4096

typedef struct _autofade2mc{
    t_object  x_obj;
    int       x_n;
    int       x_n_chans;
    int       x_fadein, x_fadeout;              // fade size in samples
    int       x_nleft;                          // n samples left
    float     x_ksr, x_fadein_ms, x_fadeout_ms, x_inc;
    t_float  *x_input;                         // inputs copy
    t_float  *x_gate;                           // gate
    t_int     x_gate_on;                        // gate status
    t_float   x_lastgate, x_lastfade, x_start, x_end, x_delta, x_phase;
    t_int     x_table;
}t_autofade2mc;

static t_class *autofade2mc_class;

static void autofade2mc_fadein(t_autofade2mc *x, t_floatarg f){
    x->x_fadein_ms = f < 0 ? 0 : f;
    x->x_fadein = (int)((x->x_fadein_ms * x->x_ksr) + 0.5);
}

static void autofade2mc_fadeout(t_autofade2mc *x, t_floatarg f){
    x->x_fadeout_ms = f < 0 ? 0 : f;
    x->x_fadeout = (int)((x->x_fadeout_ms * x->x_ksr) + 0.5);
}

static void autofade2mc_quartic(t_autofade2mc *x){
    x->x_table = 0;
}

static void autofade2mc_lin(t_autofade2mc *x){
    x->x_table = 1;
}

static void autofade2mc_linsin(t_autofade2mc *x){
    x->x_table = 2;
}

static void autofade2mc_sqrt(t_autofade2mc *x){
    x->x_table = 3;
}

static void autofade2mc_sin(t_autofade2mc *x){
    x->x_table = 4;
}

static void autofade2mc_hannsin(t_autofade2mc *x){
    x->x_table = 5;
}

static void autofade2mc_hann(t_autofade2mc *x){
    x->x_table = 6;
}

static t_int *autofade2mc_perform(t_int *w){
    t_autofade2mc *x = (t_autofade2mc *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *gate = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    t_float *ins = x->x_input;
    int i;
    for(i = 0; i < x->x_n_chans*x->x_n; i++) // copy input
        ins[i] = in[i];
    t_float lastgate = x->x_lastgate;
    t_float lastfade = x->x_lastfade;
    t_float phase = x->x_phase;
    for(int n = 0; n < x->x_n; n++){ // n sample in block
        if((gate[n] <= 0 && lastgate > 0)
        || (gate[n] > 0 && lastgate <= 0)){ // gate status changed, update
            x->x_start = lastfade;
            x->x_end = gate[n];
            x->x_delta = x->x_end - x->x_start;
            x->x_gate_on = gate[n] > 0;
            if(x->x_gate_on){ // gate on
                x->x_nleft = x->x_fadein;
                float inc = x->x_nleft > 0 ? 1./x->x_fadein : 0;
                phase = 0.;
                x->x_inc = inc;
            }
            else{ // gate off
                x->x_nleft = x->x_fadeout;
                float inc = x->x_nleft > 0 ? 1./x->x_fadeout : 0;
                phase = 1.;
                x->x_inc = -inc;
            }
        }
        lastgate = gate[n];
        if(x->x_nleft > 0){
            phase += x->x_inc;
            x->x_nleft--;
        }
        else
            phase = x->x_gate_on;
        float fadeval = read_fadetab(phase, x->x_table);
        lastfade = fadeval;
        float finalfade;
        if(x->x_gate_on)
            finalfade = x->x_start + (fadeval * x->x_delta);
        else // gate off
            finalfade = fadeval * x->x_start;
        for(i = 0; i < x->x_n_chans; i++)
            out[i*x->x_n + n] = ins[i*x->x_n + n] * finalfade;
    }
    x->x_lastgate = lastgate;
    x->x_lastfade = lastfade;
    x->x_phase = phase;
    return(w+5);
}

static void autofade2mc_dsp(t_autofade2mc *x, t_signal **sp){
    int n = sp[0]->s_n, ch1 = sp[0]->s_nchans;
    float ksr = sp[0]->s_sr * .001;
    if(ksr != x->x_ksr){
        x->x_ksr = ksr;
        autofade2mc_fadein(x, x->x_fadein_ms);
        autofade2mc_fadeout(x, x->x_fadeout_ms);
    }
    if(n != x->x_n || ch1 != x->x_n_chans){
        x->x_input = (t_float *)resizebytes(x->x_input,
            x->x_n*x->x_n_chans * sizeof(t_float), n*ch1 * sizeof(t_float));
        x->x_n = n, x->x_n_chans = ch1;
    }
    signal_setmultiout(&sp[2], ch1);
    if(sp[1]->s_nchans > 1){
        dsp_add_zero(sp[2]->s_vec, ch1*n);
        pd_error(x, "[autofade2mc~]: gate input cannot have more than one channel");
    }
    dsp_add(autofade2mc_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *autofade2mc_free(t_autofade2mc *x){
    freebytes(x->x_input, x->x_n*x->x_n_chans*sizeof(*x->x_input));
    return(void *)x;
}

static void *autofade2mc_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_autofade2mc *x = (t_autofade2mc *)pd_new(autofade2mc_class);
    init_fade_tables();
    t_float fademsin = 10., fademsout = 10.;
    x->x_n_chans = 1;
    x->x_ksr = sys_getsr() * .001;
    x->x_n = sys_getblksize();
    x->x_table = 0; // quartic
    if(ac && av->a_type == A_SYMBOL){ // optional 1st symbol arg
        t_symbol *curarg = atom_getsymbol(av);
        if(curarg == gensym("quartic"))
            x->x_table = 0;
        else if(curarg == gensym("lin"))
            x->x_table = 1;
        else if(curarg == gensym("linsin"))
            x->x_table = 2;
        else if(curarg == gensym("sqrt"))
            x->x_table = 3;
        else if(s == gensym("sin"))
            x->x_table = 4;
        else if(curarg == gensym("hannsin"))
            x->x_table = 5;
        else if(curarg == gensym("hann"))
            x->x_table = 6;
        ac--, av++;
    }
    if(ac){
        fademsin = atom_getfloat(av);
        ac--, av++;
    }
    if(ac){
        fademsout = atom_getfloat(av);
        ac--, av++;
    }
    x->x_input = (t_float *)getbytes(x->x_n*x->x_n_chans*sizeof(*x->x_input));
    x->x_end = x->x_nleft = x->x_inc = x->x_lastgate = 0.;
    autofade2mc_fadein(x, fademsin);
    autofade2mc_fadeout(x, fademsout);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void setup_autofade20x2emc_tilde(void){
    autofade2mc_class = class_new(gensym("autofade2.mc~"), (t_newmethod)autofade2mc_new,
        (t_method)autofade2mc_free, sizeof(t_autofade2mc), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(autofade2mc_class, nullfn, gensym("signal"), 0);
    class_addmethod(autofade2mc_class, (t_method)autofade2mc_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(autofade2mc_class, (t_method)autofade2mc_fadein, gensym("fadein"), A_FLOAT, 0);
    class_addmethod(autofade2mc_class, (t_method)autofade2mc_fadeout, gensym("fadeout"), A_FLOAT, 0);
    class_addmethod(autofade2mc_class, (t_method)autofade2mc_quartic, gensym("quartic"), 0);
    class_addmethod(autofade2mc_class, (t_method)autofade2mc_lin, gensym("lin"), 0);
    class_addmethod(autofade2mc_class, (t_method)autofade2mc_sqrt, gensym("sqrt"), 0);
    class_addmethod(autofade2mc_class, (t_method)autofade2mc_sin, gensym("sin"), 0);
    class_addmethod(autofade2mc_class, (t_method)autofade2mc_hann, gensym("hann"), 0);
    class_addmethod(autofade2mc_class, (t_method)autofade2mc_linsin, gensym("linsin"), 0);
    class_addmethod(autofade2mc_class, (t_method)autofade2mc_hannsin, gensym("hannsin"), 0);
}
