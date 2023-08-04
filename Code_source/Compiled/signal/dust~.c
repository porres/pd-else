// Porres 2017

#include "m_pd.h"
#include "random.h"

static t_class *dust_class;

typedef struct _dust{
    t_object       x_obj;
    t_float        x_sample_dur;
    t_random_state x_rstate;
    t_float        x_density;
    t_float       *x_lastout;
    int            x_id;
    int            x_nchans;
    int            x_ch;
}t_dust;

static void dust_seed(t_dust *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
}

static void dust_ch(t_dust *x, t_floatarg f){
    x->x_ch = f < 1 ? 1 : (int)f;
    canvas_update_dsp();
}

static t_int *dust_perform(t_int *w){
    t_dust *x = (t_dust *)(w[1]);
    int n = (t_int)(w[2]);
    int chs = (t_int)(w[3]);
    t_float *in1 = (t_float *)(w[4]);
    t_float *out = (t_sample *)(w[5]);
    t_float *lastout = x->x_lastout;
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    for(int i = 0; i < n; i++){
        for(int j = 0; j < x->x_nchans; j++){
        t_float density = chs == 1 ? in1[i] : in1[j*n + i];
        t_float thresh = density * x->x_sample_dur;
        t_float scale = thresh > 0 ? 1./thresh : 0;
            t_float random = (t_float)(random_frand(s1, s2, s3) * 0.5 + 0.5);
            t_float output = random < thresh ? random * scale : 0;
            if(output != 0 && lastout[j] != 0)
                output = 0;
            out[j*n + i] = lastout[j] = output;
        }
    }
    x->x_lastout = lastout;
    return(w+6);
}

static void dust_dsp(t_dust *x, t_signal **sp){
    x->x_sample_dur = 1./sp[0]->s_sr;
    int chs = sp[0]->s_nchans, n = sp[0]->s_n;
    if(chs == 1)
        chs = x->x_ch;
    signal_setmultiout(&sp[1], chs);
    if(x->x_nchans != chs){
        x->x_lastout = (t_float *)resizebytes(x->x_lastout,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(dust_perform, 5, x, n, sp[0]->s_nchans, sp[0]->s_vec, sp[1]->s_vec);
}

static void *dust_free(t_dust *x){
    freebytes(x->x_lastout, x->x_nchans * sizeof(*x->x_lastout));
    return(void *)x;
}

static void *dust_new(t_symbol *s, int ac, t_atom *av){
    t_dust *x = (t_dust *)pd_new(dust_class);
    x->x_id = random_get_id();
    x->x_nchans = 1;
    dust_seed(x, s, 0, NULL);
    x->x_lastout = (t_float *)getbytes(sizeof(*x->x_lastout));
    x->x_ch = 1;
    while(av->a_type == A_SYMBOL){
        if(ac >= 2 && atom_getsymbol(av) == gensym("-seed")){
            t_atom at[1];
            SETFLOAT(at, atom_getfloat(av+1));
            ac-=2, av+=2;
            dust_seed(x, s, 1, at);
        }
        else if(ac >= 2 && atom_getsymbol(av) == gensym("-ch")){
            int n = atom_getint(av+1);
            x->x_ch = n < 1 ? 1 : n;
            ac-=2, av+=2;
        }
        else{
            pd_error(x, "[dust~]: improper args");
            return(NULL);
        }
    }
    x->x_density = ac ? atom_getfloat(av) : 0;
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void dust_tilde_setup(void){
    dust_class = class_new(gensym("dust~"), (t_newmethod)dust_new,
        (t_method)dust_free, sizeof(t_dust), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(dust_class, t_dust, x_density);
    class_addmethod(dust_class, (t_method)dust_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(dust_class, (t_method)dust_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(dust_class, (t_method)dust_ch, gensym("ch"), A_DEFFLOAT, 0);
}
