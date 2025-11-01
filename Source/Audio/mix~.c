// porres 2025

#include <m_pd.h>
#include "buffer.h"
#include "magic.h"
#include "math.h"

#include <stdlib.h>

#define MAXLEN      4096 // Max input
#define NINS        3    // Number of Inlets
#define NOUTS       2    // stereo output

typedef struct _mix{
    t_object    x_obj;
    t_float   **x_ins;          // inputs
    t_float   **x_outs;         // outputs
    t_inlet    *x_inlet_gain;
    t_inlet    *x_inlet_pan;
    t_int       x_ch2, x_ch3;
    t_int       x_sig2, x_sig3;
    int         x_nchans;
    int         x_n;            // block size
    t_int       x_pan_size, x_gain_size;
    float      *x_pan_list;
    float      *x_gain_list;
    t_symbol   *x_ignore;
    t_glist    *x_glist;
    t_float    *x_signalscalar1;
    t_float    *x_signalscalar2;
}t_mix;

static t_class *mix_class;

static void mix_pan(t_mix *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_pan_size != ac){
        x->x_pan_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_pan_list[i] = atom_getfloat(av+i);
}

static void mix_gain(t_mix *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_gain_size != ac){
        x->x_gain_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_gain_list[i] = atom_getfloat(av+i);
}

static void mix_db(t_mix *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_gain_size != ac){
        x->x_gain_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++){
        float db = atom_getfloat(av+i);
        if(db <= -100)
            x->x_gain_list[i] = 0;
        else
            x->x_gain_list[i] = pow(10., db/20.);
    }
}

static t_int *mix_perform(t_int *w){
    t_mix *x = (t_mix *)(w[1]);
    if(!x->x_sig2){
        t_float *scalar = x->x_signalscalar1;
        if(!else_magic_isnan(*x->x_signalscalar1)){
            t_float gain = *scalar;
            for(int j = 0; j < x->x_nchans; j++)
                x->x_gain_list[j] = gain;
            else_magic_setnan(x->x_signalscalar1);
        }
    }
    if(!x->x_sig3){
        t_float *scalar = x->x_signalscalar2;
        if(!else_magic_isnan(*x->x_signalscalar2)){
            t_float pan = *scalar;
            for(int j = 0; j < x->x_nchans; j++)
                x->x_pan_list[j] = pan;
            else_magic_setnan(x->x_signalscalar2);
        }
    }
    for(int i = 0; i < x->x_n; i++){
        for(int j = 0; j < NOUTS; j++)
            x->x_outs[j][i] = 0;
        for(int n = 0; n < x->x_nchans; n++){
            int ch = n * x->x_n;;
            t_float in = x->x_ins[0][i+ch];
            int mc = x->x_ch2 > 1;
            t_float g = x->x_sig2 ? x->x_ins[1][i+ch*mc] : x->x_gain_list[n*mc];
            mc = x->x_ch3 > 1;
            t_float pan = x->x_sig3 ? x->x_ins[2][i+ch*mc] : x->x_pan_list[n*mc];
            pan = pan < -1 ? 0 : pan > 1 ? 1 : pan * 0.5 + 0.5;
            float pos = pan + 1;
            for(int j = 0; j < NOUTS; j++){
                float chanpos = (pos - j) * 0.5;
                chanpos -= floor(chanpos);
                float chanamp = chanpos > 1 ? 0 : read_sintab(chanpos*0.5);
                x->x_outs[j][i] += (in * chanamp) * g;
            }
        }
    };
    return(w+2);
}

static void mix_dsp(t_mix *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans, x->x_ch3 = sp[2]->s_nchans;
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    x->x_sig3 = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal);
    if(!x->x_sig2)
        x->x_ch2 = x->x_gain_size;
    if(!x->x_sig3)
        x->x_ch3 = x->x_pan_size;
    t_signal **sigp = sp;
    if(x->x_nchans != chs){
        x->x_ins = (t_float **)resizebytes(x->x_ins,
            x->x_nchans * NINS * sizeof(t_float *),
            chs * NINS * sizeof(t_float *));
        x->x_nchans = chs;
    }
    int i;
    for(i = 0; i < NINS; i++) // inlets
        *(x->x_ins+i) = (*sigp++)->s_vec;
    for(i = 0; i < NOUTS; i++){ // outlets
        signal_setmultiout(&sp[NINS+i], 1);
        *(x->x_outs+i) = (*sigp++)->s_vec;
    }
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)){
        for(i = 0; i < NOUTS; i++)
            dsp_add_zero(sp[NINS+i]->s_vec, x->x_n);
        pd_error(x, "[mix~]: channel sizes mismatch");
        return;
    }
    else
        dsp_add(mix_perform, 1, x);
}

void *mix_free(t_mix *x){
    freebytes(x->x_ins, NINS * x->x_nchans * sizeof(*x->x_ins));
    freebytes(x->x_outs, NOUTS * x->x_nchans * sizeof(*x->x_outs));
    free(x->x_gain_list);
    free(x->x_pan_list);
    inlet_free(x->x_inlet_gain);
    inlet_free(x->x_inlet_pan);
    return(void *)x;
}

static void *mix_new(t_symbol *s, int ac, t_atom *av){
    t_mix *x = (t_mix *)pd_new(mix_class);
    x->x_ignore = s;
    x->x_glist = canvas_getcurrent();
    init_sine_table();
    x->x_pan_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_gain_list = (float*)malloc(MAXLEN * sizeof(float));
/*    for(int i = 0; i < MAXLEN; i++){
        x->x_gain_list[i] = 1, x->x_pan_list[i] = 0;
    }*/
    x->x_gain_list[0] = 1, x->x_pan_list[0] = 0;
    x->x_pan_size = x->x_gain_size = 1;
    x->x_nchans = 1;
    while(ac){
        if(av->a_type == A_SYMBOL){
            t_symbol *sym = atom_getsymbol(av);
            if(sym == gensym("-gain")){
                ac--, av++;
                if(!ac || av->a_type == A_SYMBOL)
                    goto errstate;
                int i = 0;
                while(ac && av->a_type == A_FLOAT){
                    x->x_gain_list[i] = atom_getfloat(av);
                    ac--, av++, i++;
                }
                x->x_gain_size = i;
            }
            else if(sym == gensym("-db")){
                ac--, av++;
                if(!ac || av->a_type == A_SYMBOL)
                    goto errstate;
                int i = 0;
                while(ac && av->a_type == A_FLOAT){
                    float db = atom_getfloat(av);
                    if(db <= -100)
                        x->x_gain_list[i] = 0;
                    else
                        x->x_gain_list[i] = pow(10., db/20.);
                    ac--, av++, i++;
                }
                x->x_gain_size = i;
            }
            else if(sym == gensym("-pan")){
                ac--, av++;
                if(!ac || av->a_type == A_SYMBOL)
                    goto errstate;
                int i = 0;
                while(ac && av->a_type == A_FLOAT){
                    x->x_pan_list[i] = atom_getfloat(av);
                    ac--, av++, i++;
                }
                x->x_pan_size = i;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    x->x_ins = getbytes(NINS * sizeof(*x->x_ins));
    x->x_outs = getbytes(NOUTS * sizeof(*x->x_outs));
    x->x_inlet_gain = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
//        pd_float((t_pd *)x->x_inlet_gain, x->x_gain_list[0]);
    x->x_inlet_pan = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
//        pd_float((t_pd *)x->x_inlet_pan, x->x_pan_list[0]);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    x->x_signalscalar1 = obj_findsignalscalar((t_object *)x, 1);
    x->x_signalscalar2 = obj_findsignalscalar((t_object *)x, 2);
    else_magic_setnan(x->x_signalscalar1);
    else_magic_setnan(x->x_signalscalar2);
    return(x);
errstate:
    pd_error(x, "[mix~]: improper args");
    return(NULL);
}

void mix_tilde_setup(void){
    mix_class = class_new(gensym("mix~"), (t_newmethod)mix_new,
        (t_method)mix_free, sizeof(t_mix), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(mix_class, nullfn, gensym("signal"), 0);
    class_addmethod(mix_class, (t_method)mix_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(mix_class, (t_method)mix_pan, gensym("pan"), A_GIMME, 0);
    class_addmethod(mix_class, (t_method)mix_gain, gensym("gain"), A_GIMME, 0);
    class_addmethod(mix_class, (t_method)mix_db, gensym("db"), A_GIMME, 0);
}
