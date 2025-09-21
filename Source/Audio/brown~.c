// Matt Barber and Porres (2017-2022)

#include <m_pd.h>
#include <g_canvas.h>
#include "magic.h"
#include "random.h"
#include <stdlib.h>

static t_class *brown_class;

typedef struct _brown{
    t_object       x_obj;
    t_random_state x_rstate;
    t_glist       *x_glist;
    t_float       *x_lastout;
    t_float        x_step;
    t_float        x_inmode;
    int            x_id;
    int            x_chs;
    int            x_nblock;
    int            x_nchans;
}t_brown;

static void brown_seed(t_brown *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
}

static void brown_ch(t_brown *x, t_floatarg f){
    x->x_chs = f < 1 ? 1 : (int)f;
    canvas_update_dsp();
}

static void brown_step(t_brown *x, t_floatarg f){
    x->x_step = f < 0 ? 0 : f > 1 ? 1 : f;
}

static t_int *brown_perform(t_int *w){
    t_brown *x = (t_brown *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    t_float *lastout = x->x_lastout;
    int n = x->x_nblock;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            if(x->x_inmode){
                t_float impulse = (in[j*n + i] != 0);
                if(impulse){
                    t_float noise = random_frand(s1, s2, s3);
                    lastout[j] += (noise * x->x_step);
                    if(lastout[j] > 1)
                        lastout[j] = 2 - lastout[j];
                    if(lastout[j] < -1)
                        lastout[j] = -2 - lastout[j];
                }
                out[j*n + i] = lastout[j];
            }
            else{
                t_float noise = random_frand(s1, s2, s3);
                lastout[j] += (noise * x->x_step);
                if(lastout[j] > 1)
                    lastout[j] = 2 - lastout[j];
                if(lastout[j] < -1)
                    lastout[j] = -2 - lastout[j];
                out[j*n + i] = lastout[j];
            }
        }
    }
    x->x_lastout = lastout;
    return(w+4);
}

static void brown_dsp(t_brown *x, t_signal **sp){
    x->x_inmode = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    x->x_nblock = sp[0]->s_n;
    int chs = x->x_inmode ? sp[0]->s_nchans : x->x_chs;
    if(x->x_nchans != chs){
       x->x_lastout = (t_float *)resizebytes(x->x_lastout,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(brown_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void *brown_free(t_brown *x){
    freebytes(x->x_lastout, x->x_nchans * sizeof(*x->x_lastout));
    return(void *)x;
}

static void *brown_new(t_symbol *s, int ac, t_atom *av){
    t_brown *x = (t_brown *)pd_new(brown_class);
    x->x_id = random_get_id();
    x->x_glist = (t_glist *)canvas_getcurrent();
    x->x_step = 0.125;
    x->x_chs = 1;
    x->x_lastout = (t_float *)getbytes(sizeof(*x->x_lastout));
    x->x_lastout[0] = 0;
    brown_seed(x, s, 0, NULL);
    while(ac && av->a_type == A_SYMBOL){
        if(av->a_type == A_SYMBOL){
            t_symbol *sym = atom_getsymbol(av);
            if(sym == gensym("-seed")){
                if(ac >= 2){
                    t_atom at[1];
                    SETFLOAT(at, atom_getfloat(av+1));
                    ac-=2, av+=2;
                    brown_seed(x, s, 1, at);
                }
                else
                    goto errstate;
            }
            else if(sym == gensym("-ch")){
                if(ac >= 2){
                    int n = atom_getint(av+1);
                    brown_ch(x, n < 1 ? 1 : n);
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
    }
    if(ac && av->a_type == A_FLOAT)
        brown_step(x, atom_getfloat(av));
    outlet_new(&x->x_obj, &s_signal);
    return(x);
    errstate:
        pd_error(x, "[brown~]: improper args");
        return(NULL);
}

void brown_tilde_setup(void){
    brown_class = class_new(gensym("brown~"), (t_newmethod)(void *)brown_new,
        (t_method)brown_free, sizeof(t_brown), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addfloat(brown_class, brown_step);
    class_addmethod(brown_class, nullfn, gensym("signal"), 0);
    class_addmethod(brown_class, (t_method)brown_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(brown_class, (t_method)brown_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(brown_class, (t_method)brown_ch, gensym("ch"), A_FLOAT, 0);
}
