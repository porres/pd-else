// Porres 2017-2024

#include <m_pd.h>
#include "random.h"

#define PINK_MAX_OCT 40
#define MAXLEN 1024

static t_class *pink_class;

typedef struct _pink{
    t_object       x_obj;
    t_random_state x_rstate;
    float          x_signals[PINK_MAX_OCT][MAXLEN];
    int            x_id;
    int            x_ch;
    int            x_n;
    float          x_sr;
    t_float       *x_total;
    int            x_octaves_set;
    int            x_octaves;
}t_pink;

static void pink_init(t_pink *x){
    for(int c = 0; c < x->x_ch; c++)
        x->x_total[c] = 0.0f;
    t_random_state *rstate = &x->x_rstate;
    uint32_t *s1 = &rstate->s1;
    uint32_t *s2 = &rstate->s2;
    uint32_t *s3 = &rstate->s3;
    for(int i = 0; i < x->x_octaves - 1; ++i){
        for(int j = 0; j < x->x_ch; j++){
            float noise = (random_frand(s1, s2, s3));
            x->x_total[j] += noise;
            x->x_signals[i][j] = noise;
        }
    }
}

static void pink_seed(t_pink *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
    pink_init(x);
}

static void pink_oct(t_pink *x, t_floatarg f){
    x->x_octaves = (int)f  < 1 ? 1 : (int)f > PINK_MAX_OCT ? PINK_MAX_OCT : (int)f;
    x->x_octaves_set = 0;
    pink_init(x);
}

static void pink_ch(t_pink *x, t_floatarg f){
    int ch = f < 1 ? 1 : f > MAXLEN ? MAXLEN : (int)f;
    if(x->x_ch != ch){
        x->x_total = (t_float *)resizebytes(x->x_total,
            x->x_ch * sizeof(t_float), ch * sizeof(t_float));
        x->x_ch = ch;
    }
    canvas_update_dsp();
}

static t_int *pink_perform(t_int *w){
    t_pink *x = (t_pink *)(w[1]);
    t_float *out = (t_sample *)(w[2]);
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    float *total = x->x_total;
    for(int i = 0; i < x->x_n; i++){
        for(int j = 0; j < x->x_ch; j++){
            uint32_t rcounter = random_trand(s1, s2, s3);
            float newrand = random_frand(s1, s2, s3);
            int k = (CLZ(rcounter));
            if(k < (x->x_octaves-1)){
                float prevrand = x->x_signals[k][j];
                x->x_signals[k][j] = newrand;
                total[j] += (newrand - prevrand);
            }
            newrand = (random_frand(s1, s2, s3));
            out[j*x->x_n + i] = (t_float)(total[j]+newrand)/x->x_octaves;
        }
    }
    x->x_total = total;
    return(w+3);
}

static void pink_dsp(t_pink *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    if(x->x_octaves_set && x->x_sr != sp[0]->s_sr){
        t_float sr = x->x_sr = sp[0]->s_sr;
        x->x_octaves = 1;
        while(sr >= 40){
            sr *= 0.5;
            x->x_octaves++;
        }
        pink_init(x);
    }
    signal_setmultiout(sp, x->x_ch);
    dsp_add(pink_perform, 2, x, sp[0]->s_vec);
}

static void *pink_free(t_pink *x){
    freebytes(x->x_total, x->x_ch * sizeof(*x->x_total));
    return(void *)x;
}

static void *pink_new(t_symbol *s, int ac, t_atom *av){
    t_pink *x = (t_pink *)pd_new(pink_class);
    x->x_id = random_get_id();
    x->x_total = (t_float *)getbytes(sizeof(*x->x_total));
    x->x_total[0] = 0;
    x->x_ch = 1;
    pink_seed(x, s, 0, NULL);
    x->x_sr = 0;
    if(ac){
        while(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-seed")){
                if(ac >= 2){
                    t_atom at[1];
                    SETFLOAT(at, atom_getfloat(av+1));
                    ac-=2, av+=2;
                    pink_seed(x, s, 1, at);
                }
                else{
                    pd_error(x, "[pink~]: -seed needs a seed value");
                    return(NULL);
                }
            }
            else if(atom_getsymbol(av) == gensym("-ch")){
                if(ac >= 2){
                    int n = atom_getint(av+1);
                    pink_ch(x, n < 1 ? 1 : n);
                    ac-=2, av+=2;
                }
                else{
                    pd_error(x, "[pink~]: -ch needs a channel number value");
                    return(NULL);
                }
            }
            else{
                pd_error(x, "[pink~]: improper flag (%s)", atom_getsymbol(av)->s_name);
                return(NULL);
            }
        }
    }
    if(ac && av->a_type == A_FLOAT)
        pink_oct(x, atom_getfloat(av));
    else
        x->x_octaves_set = 1;
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void pink_tilde_setup(void){
    pink_class = class_new(gensym("pink~"), (t_newmethod)pink_new,
        (t_method)pink_free, sizeof(t_pink), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(pink_class, (t_method)pink_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(pink_class, pink_oct);
    class_addmethod(pink_class, (t_method)pink_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(pink_class, (t_method)pink_ch, gensym("ch"), A_FLOAT, 0);
}
