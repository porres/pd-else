// Porres 2017

#include "m_pd.h"
#include "random.h"
#include <stdlib.h>

static t_class *randi_class;

typedef struct _randi{
    t_object        x_obj;
    t_random_state  x_rstate;
    t_int          *x_randi;
    t_float        *x_lastin;
    t_inlet        *x_low_let;
    t_inlet        *x_high_let;
    int             x_id;
    int             x_nchans;
    int             x_ch;
    int             x_trig_bang;
}t_randi;

static void randi_seed(t_randi *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
}

static void randi_bang(t_randi *x){
    x->x_trig_bang = 1;
}

static void randi_ch(t_randi *x, t_floatarg f){
    x->x_ch = f < 1 ? 1 : (int)f;
    canvas_update_dsp();
}

static t_int *randi_perform(t_int *w){
    t_randi *x = (t_randi *)(w[1]);
    int n = (t_int)(w[2]);
    int chs = (t_int)(w[3]);
    t_float *in1 = (t_float *)(w[4]);
    t_float *in2 = (t_float *)(w[5]);
    t_float *in3 = (t_float *)(w[6]);
    t_float *out = (t_sample *)(w[7]);
    t_float *lastin = x->x_lastin;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float out_low = (int)(in2[i]);
            float out_high = (int)(in3[i]);
            if(out_low > out_high){
                int temp = out_low;
                out_low = out_high;
                out_high = temp;
            }
            int range = out_high - out_low; // Range
            float trig = chs == 1 ? in1[i] : in1[j*n + i];
            if(range == 0)
                x->x_randi[j] = out_low;
            else{
                t_int trigger = 0;
                if(x->x_trig_bang){
                    trigger = 1;
                    if(j == (x->x_nchans - 1))
                        x->x_trig_bang = 0;
                }
                else
                    trigger = (trig > 0 && lastin[j] <= 0) || (trig < 0 && lastin[j] >= 0);
                if(trigger){ // update
                    uint32_t *s1 = &x->x_rstate.s1;
                    uint32_t *s2 = &x->x_rstate.s2;
                    uint32_t *s3 = &x->x_rstate.s3;
                    t_float noise = (t_float)(random_frand(s1, s2, s3)) * 0.5 + 0.5;
                    x->x_randi[j] = (int)((noise * (range + 1))) + out_low;
                }
            }
            out[j*n + i] = x->x_randi[j];
            lastin[j] = trig;
        }
    }
    x->x_lastin = lastin; // last input
    return(w+8);
}

static void randi_dsp(t_randi *x, t_signal **sp){
    int chs = sp[0]->s_nchans, n = sp[0]->s_n;
    if(chs == 1)
        chs = x->x_ch;
    signal_setmultiout(&sp[3], chs);
    if(x->x_nchans != chs){
        x->x_randi = (t_int *)resizebytes(x->x_randi,
            x->x_nchans * sizeof(t_int), chs * sizeof(t_int));
        x->x_lastin = (t_float *)resizebytes(x->x_lastin,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    dsp_add(randi_perform, 7, x, n, sp[0]->s_nchans,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *randi_free(t_randi *x){
    inlet_free(x->x_low_let);
    inlet_free(x->x_high_let);
    freebytes(x->x_randi, x->x_nchans * sizeof(*x->x_randi));
    freebytes(x->x_lastin, x->x_nchans * sizeof(*x->x_lastin));
    return(void *)x;
}

static void *randi_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_randi *x = (t_randi *)pd_new(randi_class);
    x->x_id = random_get_id();
    x->x_nchans = 1;
    x->x_randi = (t_int *)getbytes(sizeof(*x->x_randi));
    x->x_lastin = (t_float *)getbytes(sizeof(*x->x_lastin));
    randi_seed(x, s, 0, NULL);
    float low = 0, high = 1;
    x->x_ch = 1;
    while(av->a_type == A_SYMBOL){
        if(ac >= 2 && atom_getsymbol(av) == gensym("-seed")){
            t_atom at[1];
            SETFLOAT(at, atom_getfloat(av+1));
            ac-=2, av+=2;
            randi_seed(x, s, 1, at);
        }
        else if(ac >= 2 && atom_getsymbol(av) == gensym("-ch")){
            int n = atom_getint(av+1);
            x->x_ch = n < 1 ? 1 : n;
            ac-=2, av+=2;
        }
        else
            goto errstate;
    }
    if(ac && av->a_type == A_FLOAT){
        low = atom_getintarg(0, ac, av);
        ac--, av++;
        if(ac && av->a_type == A_FLOAT){
            high = atom_getintarg(0, ac, av);
            ac--, av++;
        }
    }
    x->x_low_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_low_let, low);
    x->x_high_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_high_let, high);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
    errstate:
        pd_error(x, "[rand.i~]: improper args");
        return(NULL);
}

void setup_rand0x2ei_tilde(void){
    randi_class = class_new(gensym("rand.i~"), (t_newmethod)randi_new,
        (t_method)randi_free, sizeof(t_randi), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(randi_class, nullfn, gensym("signal"), 0);
    class_addmethod(randi_class, (t_method)randi_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(randi_class, (t_method)randi_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(randi_class, (t_method)randi_ch, gensym("ch"), A_DEFFLOAT, 0);
    class_addbang(randi_class, (t_method)randi_bang);
}
