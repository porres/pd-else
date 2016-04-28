
//

#include "m_pd.h"

typedef struct _imp
{
    t_object      x_obj;
    t_float    x_freq;
    double     x_phase;
    t_inlet    *x_phaselet;
    t_outlet   *x_outlet;
} t_imp;

static t_class *imp_class;


static t_int *imp_perform(t_int *w)
{
    t_imp *x = (t_imp *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float *tab = x->x_table;
    double dphase = x->x_phase;
    double wrapphase, tabphase, frac;
    t_float f1, f2, freq, phasein;
    double df1, df2;
    int intphase, index;
    int imp_tabsize = x->x_imp_tabsize;
    
    while (nblock--)
    {
        freq = *in1++;
        phasein = *in2++;
        wrapphase = dphase + phasein;
        if (wrapphase>=1.)
        {
            intphase = (int)wrapphase;
            wrapphase -= intphase;
        }
        else if (wrapphase<=0.)
        {
            intphase = (int)wrapphase;
            intphase--;
            wrapphase -= intphase;
        }
        
        tabphase = wrapphase * imp_tabsize;
        index = (int)tabphase;
        frac = tabphase - index;
        if (x->x_name)
        {
            f1 = tab[index++];
            f2 = tab[index];
            *out++ = f1 + frac * (f2 - f1);
        }
        else
        {
            df1 = costab[index++];
            df2 = costab[index];
            *out++ = (t_float) (df1 + frac * (df2 - df1));
        }
        dphase += freq * conv ;
    }
    
    if (dphase>=1.)
    {
        intphase = (int)dphase;
        dphase -= intphase;
    }
    else if (dphase<=0.)
    {
        intphase = (int)dphase;
        intphase--;
        dphase -= intphase;
    }
    x->x_phase = dphase;
    return (w + 6);
}

static void imp_dsp(t_imp *x, t_signal **sp)
{
    x->x_conv = 1.0 / sp[0]->s_sr;
    x->x_phase = 0.;
    dsp_add(imp_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *imp_new(t_symbol *s, int argc, t_atom *argv)
{
    t_imp *x = (t_imp *)pd_new(imp_class);

    t_float phase, freq, offset, bufsz;
    
    phase = PDCYimp_PHASE;
    freq = PDCYimp_FREQ;
 
    int argnum = 0;
    int anamedef = 0; //flag if array name is defined
    int pastargs = 0; //flag if attributes are specified, then don't accept array name anymore
    while(argc > 0){
        if(argv -> a_type == A_FLOAT){
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum){
                case 0:
                    freq = argval;
                    break;
                case 1:
                    offset = argval;
                    break;
                default:
                    break;
            };
            argc--;
            argv++;
            argnum++;
        }
    }
    
    x->x_phaselet = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_phaselet, phase);
    x->x_outlet = outlet_new(&x->x_obj, gensym("signal"));

    x->x_freq = freq;
    x->x_phase = phase;
    return (x);
}


static void *imp_free(t_imp *x)
{
    outlet_free(x->x_outlet);
    inlet_free(x->x_phaselet);
    return (void *)x;
}

void imp_tilde_setup(void)
{    
    imp_class = class_new(gensym("imp~"),
                            (t_newmethod)imp_new, (t_method)imp_free,
                            sizeof(t_imp), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(imp_class, t_imp, x_freq); 
    class_addmethod(imp_class, (t_method)imp_dsp, gensym("dsp"), A_CANT, 0);

}
