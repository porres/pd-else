// Porres 2017

#include "m_pd.h"

static t_class *tdelay_class;

typedef struct _tdelay{
    t_object        x_obj;
    t_inlet         *x_hz_let;
    t_outlet        *x_outlet;
    float           x_sr;
    float     x_sum;
    float     x_last_trig;
} t_tdelay;

static t_int *tdelay_perform(t_int *w){
    t_tdelay *x = (t_tdelay *)(w[1]);
    int n = (int)(w[2]);
    t_float *xin = (t_float *)(w[3]);
    t_float *hz_in = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float sr = x->x_sr;
    t_float last_trig = x->x_last_trig;
    t_float sum = x->x_sum;
    
    int i;
    for(i=0; i<n;i++){
        
        double input = (double)xin[i];
        
        double output;
        
        t_float trig = t_in[i];
        t_float hz = hz_in[i];
        if(hz < 1)
            hz = 1;
        
        float period = 1./hz;
        
        float delms = period * 1000;
        
        t_int samps = (int)roundf(period * sr);
        
        double fb_del = tdelay_read_delay(x, x->x_ybuf, samps); // get delayed vals
        if (ain[i] == 0)
            ain[i] = 0;
        else
            ain[i] = copysign(exp(log(0.001) * delms/fabs(ain[i])), ain[i]);
        
        if (trig > 0 && last_trig <= 0)
            sum = 0;
        
// gate
        t_int gate = (sum += 1) <= samps;

// output
        output = envelope + (double)ain[i] * fb_del;
        out[i] = output;

        last_trig = trig;
    };
    x->x_sum = sum; // next
    x->x_last_trig = last_trig;
    return (w + 6);
}

static void tdelay_dsp(t_tdelay *x, t_signal **sp)
{
    int sr = sp[0]->s_sr;
    if(sr != x->x_sr)
        x->x_sr = sr;
    dsp_add(tdelay_perform, 5, x, sp[0]->s_n, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec);
}


static void *tdelay_new(t_symbol *s, int argc, t_atom *argv){
    t_tdelay *x = (t_tdelay *)pd_new(tdelay_class);
/////////////////////////////////////////////////////////////////////////////////////
    float freq = 0;
////
    int argnum = 0;
    while(argc > 0){
        if(argv -> a_type == A_FLOAT)
        { //if current argument is a float
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum)
            {
                case 0:
                    freq = argval;
                    break;
                case 1:
                    decay = argval;
                    break;
                case 2:
                    cut_freq = argval;
                    break;
                default:
                    break;
            };
            argnum++;
            argc--;
            argv++;
        }
    else if (argv -> a_type == A_SYMBOL)
        goto errstate;
    };
/////////////////////////////////////////////////////////////////////////////////////
    x->x_sr = sys_getsr();
    x->x_alloc = x->x_last_trig = 0;
    x->x_xnm1 = x->x_xnm2 = x->x_ynm1 = x->x_ynm2 = 0.;
    x->x_sum =  tdelay_MAXD;
    x->x_sz = tdelay_STACK;
// clear out stack buf, set pointer to stack
    x->x_ybuf = x->x_fbstack;
    tdelay_clear(x);
    if (freq < 0)
        freq = 0;
    x->x_maxdel = 1000;
// ship off to the helper method to deal with allocation if necessary
    tdelay_sz(x);
    
// inlets / outlet
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_hz_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_hz_let, freq);
    x->x_alet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_alet, decay);
    x->x_inlet_cutoff = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_cutoff, cut_freq);
    x->x_outlet = outlet_new((t_object *)x, &s_signal);
    return (x);
    errstate:
        pd_error(x, "tdelay~: improper args");
        return NULL;
}

static void * tdelay_free(t_tdelay *x){
    if(x->x_alloc)
        free(x->x_ybuf);
    inlet_free(x->x_hz_let);
    inlet_free(x->x_alet);
    inlet_free(x->x_inlet_cutoff);
    outlet_free(x->x_outlet);
    return (void *)x;
}

void tdelay_tilde_setup(void)
{
    tdelay_class = class_new(gensym("tdelay~"), (t_newmethod)tdelay_new,
                (t_method)tdelay_free, sizeof(t_tdelay), 0, A_GIMME, 0);
    class_addmethod(tdelay_class, nullfn, gensym("signal"), 0);
    class_addmethod(tdelay_class, (t_method)tdelay_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(tdelay_class, (t_method)tdelay_clear, gensym("clear"), 0);
}
        
