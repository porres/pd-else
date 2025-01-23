// Porres 2017-2024

#include <m_pd.h>
#include "random.h"

static t_class *white_class;

typedef struct _white{
    t_object       x_obj;
    int            x_clip;
    t_random_state x_rstate;
    int            x_id;
    int            x_ch;
    int            x_n;
}t_white;

static void white_seed(t_white *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
}

static void white_clip(t_white *x, t_floatarg f){
    x->x_clip = f != 0;
}

static void white_ch(t_white *x, t_floatarg f){
    x->x_ch = f < 1 ? 1 : (int)f;
    canvas_update_dsp();
}

static t_int *white_perform(t_int *w){
    t_white *x = (t_white *)(w[1]);
    t_float *out = (t_sample *)(w[2]);
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    for(int i = 0; i < x->x_n; i++){
        for(int j = 0; j < x->x_ch; j++){ // for 'n' out channels
            t_float noise = (t_float)(random_frand(s1, s2, s3));
            if(x->x_clip)
                noise = noise > 0 ? 1 : -1;
            out[j*x->x_n + i] = noise;
        }
    }
    return(w+3);
}

static void white_dsp(t_white *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    signal_setmultiout(sp, x->x_ch);
    dsp_add(white_perform, 2, x, sp[0]->s_vec);
}

static void *white_new(t_symbol *s, int ac, t_atom *av){
    t_white *x = (t_white *)pd_new(white_class);
    x->x_id = random_get_id();
    x->x_ch = 1;
    white_seed(x, s, 0, NULL);
    x->x_clip = 0;
    if(ac){
        while(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-seed")){
                if(ac >= 2){
                    t_atom at[1];
                    SETFLOAT(at, atom_getfloat(av+1));
                    ac-=2, av+=2;
                    white_seed(x, s, 1, at);
                }
                else{
                    pd_error(x, "[white~]: -seed needs a seed value");
                    return(NULL);
                }
            }
            else if(atom_getsymbol(av) == gensym("-clip")){
                x->x_clip = 1;
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-ch")){
                if(ac >= 2){
                    int n = atom_getint(av+1);
                    x->x_ch = n < 1 ? 1 : n;
                    ac-=2, av+=2;
                }
                else{
                    pd_error(x, "[white~]: -ch needs a channel number value");
                    return(NULL);
                }
            }
            else{
                pd_error(x, "[white~]: improper flag (%s)", atom_getsymbol(av)->s_name);
                return(NULL);
            }
        }
    }
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void white_tilde_setup(void){
    white_class = class_new(gensym("white~"), (t_newmethod)white_new,
        0, sizeof(t_white), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(white_class, (t_method)white_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(white_class, (t_method)white_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(white_class, (t_method)white_ch, gensym("ch"), A_FLOAT, 0);
    class_addmethod(white_class, (t_method)white_clip, gensym("clip"), A_FLOAT, 0);
}
