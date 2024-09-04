// porres 2024

#include <m_pd.h>
#include <buffer.h>

#define MAXINTPUT 4096

typedef struct _xselect2{
    t_object    x_obj;
    t_float   **x_ins;
    t_float    *x_ch_select;
    t_float    *x_out;
    t_float    *x_spread;       // spread signal
    t_inlet    *x_inlet_spread;
    int 	    x_n_ins;
    int 	    x_indexed;
    int         x_circular;
    int         x_n;
}t_xselect2;

static t_class *xselect2_class;

static void xselect2_index(t_xselect2 *x, t_floatarg f){
    x->x_indexed = f != 0;
}

static void xselect2_circular(t_xselect2 *x, t_floatarg f){
    x->x_circular = f != 0;
}

static t_int *xselect2_perform(t_int *w){
    t_xselect2 *x = (t_xselect2 *)(w[1]);
    for(int i = 0; i < x->x_n; i++){
        t_float output = 0;
        t_float pos = x->x_ch_select[i];
        t_float spread = x->x_spread[i];
        if(spread < 0.1)
            spread = 0.1;
        if(!x->x_indexed)
            pos *= (x->x_n_ins - !x->x_circular);
        if(x->x_circular){
            while(pos < 0)
                pos += 1;
            while(pos > x->x_n_ins)
                pos -= x->x_n_ins;
        }
        pos += spread;
        spread *= 2;
        int j;
        if(x->x_circular){
            float range = (float)x->x_n_ins / spread;
            for(j = 0; j < x->x_n_ins; j++){
                float chanpos = (pos - j) / spread;
                chanpos = chanpos - range * floor(chanpos/range);
                float g = chanpos >= 1 ? 0 : read_sintab(chanpos*0.5);
                output += x->x_ins[j][i] * g;
            }
        }
        else for(j = 0; j < x->x_n_ins; j++){
            float chanpos = (pos - j) / spread;
            if(chanpos >= 1 || chanpos < 0)
                chanpos = 0;
            output += x->x_ins[j][i] * read_sintab(chanpos*0.5);
        }
        x->x_out[i] = output;
    };
    return(w+2);
}

static void xselect2_dsp(t_xselect2 *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    t_signal **sigp = sp;
    for(int i = 0; i < x->x_n_ins; i++)     // n_ins
        *(x->x_ins+i) = (*sigp++)->s_vec;
    x->x_ch_select = (*sigp++)->s_vec;      // idx
    x->x_spread = (*sigp++)->s_vec;         // the spread inlet
    x->x_out = (*sigp++)->s_vec;            // outlet
    dsp_add(xselect2_perform, 1, x);
}

static void *xselect2_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_xselect2 *x = (t_xselect2 *)pd_new(xselect2_class);
    init_sine_table();
    t_float n_ins = 2; // inlets not counting channel selection
    x->x_indexed = x->x_circular = 0;
    float spread = 1;
    if(ac){
        while(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-index")){
                x->x_indexed = 1;
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-circular")){
                x->x_circular = 1;
                ac--, av++;
            }
            else
                goto errstate;
        };
        if(ac){
            n_ins = atom_getint(av);
            ac--, av++;
            if(ac)
                spread = atom_getfloat(av);
        }
    };
    if(n_ins < 2)
        n_ins = 2;
    else if(n_ins > (t_float)MAXINTPUT)
        n_ins = MAXINTPUT;
    x->x_n_ins = (int)n_ins;
    x->x_ins = getbytes(n_ins * sizeof(*x->x_ins));
    for(int i = 0; i < n_ins; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_inlet_spread = inlet_new(&x->x_obj, &x->x_obj.ob_pd,  &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_spread, spread);
    outlet_new((t_object *)x, &s_signal);
    return(x);
    errstate:
        pd_error(x, "[xselect2~]: improper args");
        return(NULL);
}

void * xselect2_free(t_xselect2 *x){
    freebytes(x->x_ins, x->x_n_ins * sizeof(*x->x_ins));
    inlet_free(x->x_inlet_spread);
    return(void *)x;
}

void xselect2_tilde_setup(void){
    xselect2_class = class_new(gensym("xselect2~"), (t_newmethod)xselect2_new,
        (t_method)xselect2_free, sizeof(t_xselect2), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(xselect2_class, (t_method)xselect2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(xselect2_class, nullfn, gensym("signal"), 0);
    class_addmethod(xselect2_class, (t_method)xselect2_index, gensym("index"), A_FLOAT, 0);
    class_addmethod(xselect2_class, (t_method)xselect2_circular, gensym("circular"), A_FLOAT, 0);
}
