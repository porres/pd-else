// porres 2017-2024

#include <m_pd.h>
#include <buffer.h>

#define MAX_INOUT 4096

typedef struct _autofademc{
    t_object  x_obj;
    int       x_n;
    int       x_n_chans;
    int       x_fade;                           // fade size in samples
    int       x_nleft;                          // n samples left
    float     x_ksr, x_fade_ms, x_inc;          // sr in khz, fade size in ms and inc step
    t_float  *x_input;                         // inputs copy
    t_float  *x_gate;                           // gate
    t_int     x_gate_on;                        // gate status
    t_float   x_lastgate, x_lastfade, x_start, x_end, x_delta, x_phase;
    t_int     x_table;
}t_autofademc;

static t_class *autofademc_class;

static void autofademc_fade(t_autofademc *x, t_floatarg f){
    x->x_fade_ms = f < 0 ? 0 : f;
    x->x_fade = (int)((x->x_fade_ms * x->x_ksr) + 0.5);
}

static void autofademc_quartic(t_autofademc *x){
    x->x_table = 0;
}

static void autofademc_lin(t_autofademc *x){
    x->x_table = 1;
}

static void autofademc_linsin(t_autofademc *x){
    x->x_table = 2;
}

static void autofademc_sqrt(t_autofademc *x){
    x->x_table = 3;
}

static void autofademc_sin(t_autofademc *x){
    x->x_table = 4;
}

static void autofademc_hannsin(t_autofademc *x){
    x->x_table = 5;
}

static void autofademc_hann(t_autofademc *x){
    x->x_table = 6;
}

static t_int *autofademc_perform(t_int *w){
    t_autofademc *x = (t_autofademc *)(w[1]);
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
            x->x_nleft = x->x_fade;
            float inc = x->x_nleft > 0 ? 1./x->x_fade : 0;
            x->x_start = lastfade;
            x->x_end = gate[n];
            x->x_delta = x->x_end - x->x_start;
            x->x_gate_on = gate[n] > 0;
            if(x->x_gate_on){ // gate on
                phase = 0.;
                x->x_inc = inc;
            }
            else{ // gate off
                phase = 1.;
                x->x_inc = -inc;
            }
        }
        lastgate = gate[n];
        float fadeval = read_fadetab(phase, x->x_table);
        lastfade = fadeval;
        if(x->x_nleft > 0){
            phase += x->x_inc;
            x->x_nleft--;
        }
        else
            phase = x->x_gate_on;
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

static void autofademc_dsp(t_autofademc *x, t_signal **sp){
    int n = sp[0]->s_n, ch1 = sp[0]->s_nchans;
    float ksr = sp[0]->s_sr * .001;
    if(ksr != x->x_ksr){
        x->x_ksr = ksr;
        autofademc_fade(x, x->x_fade_ms);
    }
    if(n != x->x_n || ch1 != x->x_n_chans){
        x->x_input = (t_float *)resizebytes(x->x_input,
            x->x_n*x->x_n_chans * sizeof(t_float), n*ch1 * sizeof(t_float));
        x->x_n = n, x->x_n_chans = ch1;
    }
    signal_setmultiout(&sp[2], ch1);
    if(sp[1]->s_nchans > 1){
        dsp_add_zero(sp[2]->s_vec, ch1*n);
        pd_error(x, "[autofademc~]: gate input cannot have more than one channel");
    }
    dsp_add(autofademc_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *autofademc_free(t_autofademc *x){
    freebytes(x->x_input, x->x_n*x->x_n_chans*sizeof(*x->x_input));
    return(void *)x;
}

static void *autofademc_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_autofademc *x = (t_autofademc *)pd_new(autofademc_class);
    t_float fadems = 10.;
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
        fadems = atom_getfloat(av);
        ac--, av++;
    }
    x->x_input = (t_float *)getbytes(x->x_n*x->x_n_chans*sizeof(*x->x_input));
    x->x_end = x->x_nleft = x->x_inc = x->x_lastgate = 0.;
    autofademc_fade(x, fadems);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void setup_autofade0x2emc_tilde(void){
    autofademc_class = class_new(gensym("autofade.mc~"), (t_newmethod)autofademc_new,
        (t_method)autofademc_free, sizeof(t_autofademc), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(autofademc_class, nullfn, gensym("signal"), 0);
    class_addmethod(autofademc_class, (t_method)autofademc_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(autofademc_class, (t_method)autofademc_fade, gensym("fade"), A_FLOAT, 0);
    class_addmethod(autofademc_class, (t_method)autofademc_quartic, gensym("quartic"), 0);
    class_addmethod(autofademc_class, (t_method)autofademc_lin, gensym("lin"), 0);
    class_addmethod(autofademc_class, (t_method)autofademc_sqrt, gensym("sqrt"), 0);
    class_addmethod(autofademc_class, (t_method)autofademc_sin, gensym("sin"), 0);
    class_addmethod(autofademc_class, (t_method)autofademc_hann, gensym("hann"), 0);
    class_addmethod(autofademc_class, (t_method)autofademc_linsin, gensym("linsin"), 0);
    class_addmethod(autofademc_class, (t_method)autofademc_hannsin, gensym("hannsin"), 0);
    init_fade_tables();
}
