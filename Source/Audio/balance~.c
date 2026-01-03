// Porres 2016-2023

#include <m_pd.h>
#include <buffer.h>

static t_class *balance_class;

typedef struct _balance{
    t_object    x_obj;
    t_int       x_n;
    t_int       x_mc;
    t_inlet    *x_bal_inlet;
}t_balance;

static t_int *balance_perform(t_int *w){
    t_balance *x = (t_balance *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);   // in L
    t_float *in2 = (t_float *)(w[3]);   // in R
    t_float *in3 = (t_float *)(w[4]);   // bal
    t_float *out1 = (t_float *)(w[5]);  // L
    t_float *out2 = (t_float *)(w[6]);  // R
    int n = x->x_n;
    while(n--){
        float inL = *in1++;
        float inR = *in2++;
        float bal = (*in3++ + 1) * 0.125;
        if(bal < 0)
            bal = 0;
        if(bal > 0.25)
            bal = 0.25;
        *out1++ = inL * read_sintab(bal + 0.25);
        *out2++ = inR * read_sintab(bal);
    }
    return(w+7);
}

static t_int *balance_perform_mc(t_int *w){
    t_balance *x = (t_balance *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);   // in
    t_float *in2 = (t_float *)(w[3]);   // bal
    t_float *out = (t_float *)(w[4]);   // out
    for(int i = 0; i < x->x_n; i++){
        float bal = (in2[i] + 1) * 0.125;
        if(bal < 0)
            bal = 0;
        if(bal > 1)
            bal = 1;
        for(int j = 0; j < 2; j++){
            float in = in1[j*x->x_n + i];
            float output;
            if(j == 0)
                output = in * read_sintab(bal + 0.25);
            else
                output = in * read_sintab(bal);
            out[j*x->x_n + i] = output;
        }
    }
    return(w+5);
}

static void balance_dsp(t_balance *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    if(!x->x_mc){
        signal_setmultiout(&sp[3], 1);
        signal_setmultiout(&sp[4], 1);
        dsp_add(balance_perform, 6, x, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
    }
    else{
        signal_setmultiout(&sp[2], 2);
        if(sp[0]->s_nchans != 2){
            pd_error(x, "[balance~]: multichannel input signal must be stereo");
            dsp_add_zero(sp[2]->s_vec, 2*x->x_n);
        }
        else if(sp[1]->s_nchans != 1){
            pd_error(x, "[balance~]: balance input signal must be a single channel");
            dsp_add_zero(sp[2]->s_vec, 2*x->x_n);
        }
        else
            dsp_add(balance_perform_mc, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
    }
}

static void *balance_free(t_balance *x){
    inlet_free(x->x_bal_inlet);
    return(void *)x;
}

static void *balance_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_balance *x = (t_balance *)pd_new(balance_class);
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
    if(!x->x_mc)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym("signal"));
    x->x_bal_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_bal_inlet, f);
    outlet_new((t_object *)x, &s_signal);
    if(!x->x_mc)
        outlet_new((t_object *)x, &s_signal);
    return(x);
}

void balance_tilde_setup(void){
    balance_class = class_new(gensym("balance~"), (t_newmethod)balance_new,
        (t_method)balance_free, sizeof(t_balance), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(balance_class, nullfn, gensym("signal"), 0);
    class_addmethod(balance_class, (t_method)balance_dsp, gensym("dsp"), A_CANT, 0);
    init_sine_table();
}
