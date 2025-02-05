// Porres 2017-2024

#include <m_pd.h>
#include <stdlib.h>
#include "random.h"

#define MAXLEN 1024

static t_class *gray_class;

typedef struct _gray{
    t_object       x_obj;
    t_random_state x_rstate;
    int            x_id;
    int            x_ch;
    int            x_n;
    int           *x_base;
}t_gray;

static void gray_seed(t_gray *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
    for(int i = 0; i < x->x_ch; i++)
        x->x_base[i] = x->x_rstate.s1 ^ x->x_rstate.s2 ^ x->x_rstate.s3;
}

static void gray_ch(t_gray *x, t_floatarg f){
    int ch = f < 1 ? 1 : f > MAXLEN ? MAXLEN : (int)f;
    if(x->x_ch != ch){
        x->x_base = (int *)resizebytes(x->x_base,
            x->x_ch * sizeof(t_float), ch * sizeof(int));
        x->x_ch = ch;
        canvas_update_dsp();
    }
}

static t_int *gray_perform(t_int *w){
    t_gray *x = (t_gray *)(w[1]);
    t_float *out = (t_sample *)(w[2]);
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    int *base = x->x_base;
    for(int i = 0; i < x->x_n; i++){
        for(int j = 0; j < x->x_ch; j++){ // for 'n' out channels
            base[j] ^= 1L << (random_trand(s1, s2, s3) & 31);
            out[j*x->x_n + i] = base[j] * 4.65661287308e-10f; // That's 1/(2^31), so normalizes the int to 1.0
        }
    }
    x->x_base = base;
    return(w+3);
}

static void gray_dsp(t_gray *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    signal_setmultiout(sp, x->x_ch);
    dsp_add(gray_perform, 2, x, sp[0]->s_vec);
}

static void *gray_free(t_gray *x){
    freebytes(x->x_base, x->x_ch * sizeof(*x->x_base));
    return(void *)x;
}

static void *gray_new(t_symbol *s, int ac, t_atom *av){
    t_gray *x = (t_gray *)pd_new(gray_class);
    x->x_id = random_get_id();
    x->x_ch = 1;
    x->x_base = (int *)getbytes(sizeof(*x->x_base));
    x->x_base[0] = 0;
    gray_seed(x, s, 0, NULL);
    if(ac){
        while(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-seed")){
                if(ac >= 2){
                    t_atom at[1];
                    SETFLOAT(at, atom_getfloat(av+1));
                    ac-=2, av+=2;
                    gray_seed(x, s, 1, at);
                }
                else{
                    pd_error(x, "[gray~]: -seed needs a seed value");
                    return(NULL);
                }
            }
            else if(atom_getsymbol(av) == gensym("-ch")){
                if(ac >= 2){
                    int n = atom_getint(av+1);
                    gray_ch(x, n < 1 ? 1 : n);
                    ac-=2, av+=2;
                }
                else{
                    pd_error(x, "[gray~]: -ch needs a channel number value");
                    return(NULL);
                }
            }
            else{
                pd_error(x, "[gray~]: improper flag (%s)", atom_getsymbol(av)->s_name);
                return(NULL);
            }
        }
    }
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void gray_tilde_setup(void){
    gray_class = class_new(gensym("gray~"), (t_newmethod)gray_new,
        (t_method)gray_free, sizeof(t_gray), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(gray_class, (t_method)gray_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(gray_class, (t_method)gray_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(gray_class, (t_method)gray_ch, gensym("ch"), A_FLOAT, 0);
}
