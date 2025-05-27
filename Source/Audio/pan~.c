// porres

#include <m_pd.h>
#include "buffer.h"
#include "magic.h"

#include <stdlib.h>

#define MAXLEN 4096 // Max input
#define MAXOUTPUT 4096

typedef struct _pan{
    t_object    x_obj;
    t_float   **x_ins;          // inputs
    t_float   **x_outs;         // outputs
    t_inlet    *x_inlet_spread;
    t_inlet    *x_inlet_gain;
    t_int       x_ch2, x_ch3, x_ch4;
    t_int       x_sig2, x_sig3, x_sig4;
    int         x_nchans;
    int         x_n;            // block size
    int         x_n_outlets;    // outlets
    int         x_rad;
    int         x_open; // non circular
    t_int       x_panlist_size, x_gainlist_size, x_spreadlist_size;
    float      *x_pan_list;
    float      *x_gain_list;
    float      *x_spread_list;
    t_float     x_offset;
    t_symbol   *x_ignore;
    t_glist    *x_glist;
}t_pan;

static t_class *pan_class;

static t_int *pan_perform(t_int *w){
    t_pan *x = (t_pan *)(w[1]);
    for(int i = 0; i < x->x_n; i++){
        for(int n = 0; n < x->x_nchans; n++){
            int ch = n * x->x_n;;
            t_float in = x->x_ins[0][i+ch];
            t_float g = x->x_sig2 ? x->x_ins[1][i+ch*(x->x_ch2>1)] : x->x_gain_list[n];
            t_float pos = x->x_sig3 ? x->x_ins[2][i+ch*(x->x_ch3>1)] : x->x_pan_list[n];
            t_float spread = x->x_sig4 ? x->x_ins[3][i + ch * (x->x_ch4 > 1)] : x->x_spread_list[n];
            if(x->x_rad)
                pos /= TWO_PI;
            pos -= x->x_offset;
            while(pos < 0)
                pos += 1;
            while(pos > 1)
                pos -= 1;
            if(spread < 0.1)
                spread = 0.1;
            pos = pos * (x->x_n_outlets-x->x_open) + spread;
            spread *= 2;
            float range = x->x_n_outlets / spread;
            for(int j = 0; j < x->x_n_outlets; j++){
                float chanpos = (pos - j) / spread;
                chanpos = chanpos - range * floor(chanpos/range);
                float chanamp = chanpos > 1 ? 0 : read_sintab(chanpos*0.5);
                x->x_outs[j][i+ch] = (in * chanamp) * g;
            }
        }
    };
    return(w+2);
}

static void pan_dsp(t_pan *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    x->x_ch3 = sp[2]->s_nchans;
    x->x_ch4 = sp[3]->s_nchans;
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    x->x_sig3 = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal);
    x->x_sig4 = else_magic_inlet_connection((t_object *)x, x->x_glist, 3, &s_signal);
    if(!x->x_sig2)
        x->x_ch2 = x->x_gainlist_size;
    if(!x->x_sig3)
        x->x_ch3 = x->x_panlist_size;
    if(!x->x_sig4)
        x->x_ch4 = x->x_spreadlist_size;
    t_signal **sigp = sp;
    if(x->x_nchans != chs){
        x->x_ins = (t_float **)resizebytes(x->x_ins,
            x->x_nchans * 4 * sizeof(t_float *), chs * 4 * sizeof(t_float *));
        x->x_outs = (t_float **)resizebytes(x->x_outs,
            x->x_nchans * x->x_n_outlets * sizeof(t_float *),
            chs * x->x_n_outlets * sizeof(t_float *));
        x->x_nchans = chs;
    }
    int i;
    for(i = 0; i < 4; i++) // inlets
        *(x->x_ins+i) = (*sigp++)->s_vec;
    for(i = 0; i < x->x_n_outlets; i++){ // outlets
        signal_setmultiout(&sp[4+i], x->x_nchans);
        *(x->x_outs+i) = (*sigp++)->s_vec;
    }
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)
    || (x->x_ch4 > 1 && x->x_ch4 != x->x_nchans)){
        for(i = 0; i < x->x_n_outlets; i++)
            dsp_add_zero(sp[4+i]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[pan~]: channel sizes mismatch");
        return;
    }
    else
        dsp_add(pan_perform, 1, x);
}

static void pan_pan(t_pan *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_panlist_size != ac){
        x->x_panlist_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_pan_list[i] = atom_getfloat(av+i);
}

static void pan_gain(t_pan *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_gainlist_size != ac){
        x->x_gainlist_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_gain_list[i] = atom_getfloat(av+i);
}

static void pan_spread(t_pan *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_spreadlist_size != ac){
        x->x_spreadlist_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_spread_list[i] = atom_getfloat(av+i);
}

static void pan_offset(t_pan *x, t_floatarg f){
    x->x_offset = (f < 0 ? 0 : f) / 360;
}

static void pan_radians(t_pan *x, t_floatarg f){
    x->x_rad = (f != 0);
}

static void pan_open(t_pan *x, t_floatarg f){
    x->x_open = (f != 0);
}

void *pan_free(t_pan *x){
    freebytes(x->x_ins, 4 * x->x_nchans * sizeof(*x->x_ins));
    freebytes(x->x_outs, x->x_n_outlets * x->x_nchans * sizeof(*x->x_outs));
    inlet_free(x->x_inlet_spread);
    inlet_free(x->x_inlet_gain);
    return(void *)x;
}

static void *pan_new(t_symbol *s, int ac, t_atom *av){
    t_pan *x = (t_pan *)pd_new(pan_class);
    x->x_ignore = s;
    x->x_glist = canvas_getcurrent();
    init_sine_table();
    x->x_pan_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_gain_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_spread_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_pan_list[0] = x->x_gain_list[0] = x->x_spread_list[0] = 0;
    x->x_panlist_size = x->x_gainlist_size = x->x_spreadlist_size = 1;
    t_float n_outlets = 2;
    float spread = 1, gain = 1;
    x->x_open = x->x_rad = 0;
    x->x_offset = 0;
    int argnum = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            t_float aval = atom_getfloatarg(0, ac, av);
            switch(argnum){
                case 0:
                    n_outlets = aval;
                    break;
                case 1:
                    spread = aval;
                    break;
                case 2:
                    x->x_offset = aval / 360.;
                    break;
                default:
                    break;
            };
            argnum++, ac--, av++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            t_symbol *curarg = atom_getsymbolarg(0, ac, av);
            if(curarg == gensym("-radians")){
                x->x_rad = 1;
                ac--, av++;
            }
            else if(curarg == gensym("-open")){
                x->x_open = 1;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    if(n_outlets < 2)
        n_outlets = 2;
    else if(n_outlets > (t_float)MAXOUTPUT)
        n_outlets = MAXOUTPUT;
    x->x_n_outlets = (int)n_outlets;
    x->x_ins = getbytes(4 * sizeof(*x->x_ins));
    x->x_outs = getbytes(n_outlets * sizeof(*x->x_outs));
    x->x_inlet_gain = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_gain, gain);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal); // position
    x->x_inlet_spread = inlet_new(&x->x_obj, &x->x_obj.ob_pd,  &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_spread, spread);
    for(int i = 0; i < n_outlets; i++)
        outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "[pan~]: improper args");
    return(NULL);
}

void pan_tilde_setup(void){
    pan_class = class_new(gensym("pan~"), (t_newmethod)pan_new,
        (t_method)pan_free, sizeof(t_pan), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(pan_class, nullfn, gensym("signal"), 0);
    class_addmethod(pan_class, (t_method)pan_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(pan_class, (t_method)pan_offset, gensym("offset"), A_FLOAT, 0);
    class_addmethod(pan_class, (t_method)pan_radians, gensym("radians"), A_FLOAT, 0);
    class_addmethod(pan_class, (t_method)pan_open, gensym("open"), A_FLOAT, 0);
    class_addmethod(pan_class, (t_method)pan_pan, gensym("pan"), A_GIMME, 0);
    class_addmethod(pan_class, (t_method)pan_spread, gensym("spread"), A_GIMME, 0);
    class_addmethod(pan_class, (t_method)pan_gain, gensym("gain"), A_GIMME, 0);
}
