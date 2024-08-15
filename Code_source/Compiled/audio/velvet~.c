// Porres 2024

#include "m_pd.h"
#include "random.h"

typedef struct _velvet{
    t_object    x_obj;
    double     *x_phase;
    double     *x_lastphase;
    t_int      *x_1st;
    t_float    *x_rand;
    int         x_nchans;
    t_float     x_hz;
    t_int       x_n;
    t_int       x_ch2;
    t_int       x_ch3;
    t_inlet    *x_inlet_reg;
    t_inlet    *x_inlet_bias;
    t_outlet   *x_outlet;
    double      x_sr_rec;
    int         x_id;
    t_random_state  x_rstate;
}t_velvet;

static t_class *velvet_class;

t_float velvet_random(t_velvet *x){
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    return((t_float)(random_frand(s1, s2, s3)) * 0.5 + 0.5);
}

static void velvet_seed(t_velvet *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
    for(int j = 0; j < x->x_nchans; j++)
        x->x_rand[j] = velvet_random(x);
}

static t_int *velvet_perform(t_int *w){
    t_velvet *x = (t_velvet *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]); // bias
    t_float *in3 = (t_float *)(w[4]); // reg
    t_float *out = (t_float *)(w[5]);
    double *phase = x->x_phase;
    double *lastphase = x->x_lastphase;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            double hz = in1[j*n + i];
            double step = hz * x->x_sr_rec; // phase step
            step = step > 1 ? 1 : step < 0 ? 0 : step; // clip step
            t_float bias = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            bias = bias > 1 ? 1 : bias < 0 ? 0 : bias; // clip bias
            t_float reg = x->x_ch3 == 1 ? in3[i] : in3[j*n + i];
            reg = reg > 1 ? 1 : reg < 0 ? 0 : reg; // clip reg
            t_float imp = 0;
            if(phase[j] >= x->x_rand[j] && ((lastphase[j] < x->x_rand[j]) || (x->x_1st[j])))
                imp = velvet_random(x) > bias ? 1 : -1;
            out[j*n + i] = imp;
            x->x_1st[j] = 0;
            if(phase[j] >= 1.){
                x->x_1st[j] = 1;
                x->x_rand[j] = velvet_random(x) * (1 - reg);
                while(phase[j] >= 1.)
                    phase[j] -= 1; // wrap phase
            }
            lastphase[j] = phase[j];
            phase[j] += step;
        }
    }
    x->x_phase = phase;
    x->x_lastphase = lastphase;
    return(w+6);
}

static void velvet_dsp(t_velvet *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    int chs = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans, x->x_ch3 = sp[2]->s_nchans;
    if(x->x_nchans != chs){
        x->x_lastphase = (double *)resizebytes(x->x_lastphase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_1st = (t_int *)resizebytes(x->x_1st,
            x->x_nchans * sizeof(t_int), chs * sizeof(t_int));
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_rand = (t_float *)resizebytes(x->x_rand,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
        for(int j = 0; j < x->x_nchans; j++)
            x->x_rand[j] = velvet_random(x);
    }
    signal_setmultiout(&sp[3], x->x_nchans);
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[velvet~]: channel sizes mismatch");
        return;
    }
    dsp_add(velvet_perform, 5, x, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *velvet_free(t_velvet *x){
    inlet_free(x->x_inlet_bias);
    inlet_free(x->x_inlet_reg);
    outlet_free(x->x_outlet);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_lastphase, x->x_nchans * sizeof(*x->x_lastphase));
    freebytes(x->x_1st, x->x_nchans * sizeof(*x->x_1st));
    freebytes(x->x_rand, x->x_nchans * sizeof(*x->x_rand));
    return(void *)x;
}

static void *velvet_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_velvet *x = (t_velvet *)pd_new(velvet_class);
    x->x_id = random_get_id();
    x->x_nchans = 0;
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_lastphase = (double *)getbytes(sizeof(*x->x_lastphase));
    x->x_1st = (t_int *)getbytes(sizeof(*x->x_1st));
    x->x_rand = (t_float *)getbytes(sizeof(*x->x_rand));
    x->x_phase[0] = x->x_lastphase[0] = 0;
    x->x_hz = 0;
    float bias = 0.5;
    float reg = 0;
    velvet_seed(x, s, 0, NULL);
    if(ac){
        while(av->a_type == A_SYMBOL){
            if(ac >= 2 && atom_getsymbol(av) == gensym("-seed")){
                t_atom at[1];
                SETFLOAT(at, atom_getfloat(av+1));
                ac-=2, av+=2;
                velvet_seed(x, s, 1, at);
            }
            else
                goto errstate;
        }
        if(ac && av->a_type == A_FLOAT){
            x->x_hz = av->a_w.w_float;
            ac--, av++;
            if(ac && av->a_type == A_FLOAT){
                bias = av->a_w.w_float;
                ac--, av++;
                if(ac && av->a_type == A_FLOAT){
                    reg = av->a_w.w_float;
                    ac--, av++;
                }
            }
        }
    }
    x->x_inlet_bias = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_bias, bias);
    x->x_inlet_reg = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_reg, reg);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return(x);
errstate:
    post("[velvet~]: improper args");
    return(NULL);
}

void velvet_tilde_setup(void){
    velvet_class = class_new(gensym("velvet~"), (t_newmethod)velvet_new, (t_method)velvet_free,
        sizeof(t_velvet), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(velvet_class, t_velvet, x_hz);
    class_addmethod(velvet_class, (t_method)velvet_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(velvet_class, (t_method)velvet_seed, gensym("seed"), A_GIMME, 0);
}
