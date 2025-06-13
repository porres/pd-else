// Porres 2025

#include <m_pd.h>
#include <math.h>

#define LOG001 log(0.001)

typedef struct _follow{
    t_object    x_obj;
    void       *x_outlet;
    t_float     x_ms_up;
    t_float     x_ms_down;
    t_float     x_sr_khz;
    double     *x_last_out;
    int         x_reset;
    int         x_nchans;
    int         x_float_mode;
}t_follow;

static t_class *follow_class;

static t_int *follow_perform(t_int *w){
    t_follow *x = (t_follow *)(w[1]);
    int n = (int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    t_float sr_khz = x->x_sr_khz;
    double *last_out = x->x_last_out;
    double ms_up = x->x_ms_up, ms_down = x->x_ms_down, a, yn;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double xn = fabs(in[j*n + i]);
            if(x->x_reset){ // reset
                out[j*n + i] = last_out[j] = xn;
                if(j == (x->x_nchans - 1))
                    x->x_reset = 0;
            }
            else{
                if(xn >= last_out[j])
                    a = ms_up > 0 ? exp(LOG001 / (ms_up * sr_khz)) : 0;
                else
                    a = ms_down > 0 ? exp(LOG001 / (ms_down * sr_khz)) : 0;
                if(a == 0)
                    out[j*n + i] = yn = xn;
                else
                    out[j*n + i] = yn = xn + a*(last_out[j] - xn);
                last_out[j] = yn;
            }
        }
    }
    x->x_last_out = last_out;
    return(w+5);
}

static t_int *follow_perform_float(t_int *w){
    t_follow *x = (t_follow *)(w[1]);
    int n = (int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float sr_khz = x->x_sr_khz;
    double *last_out = x->x_last_out;
    double ms_up = x->x_ms_up, ms_down = x->x_ms_down;
    double output, a, yn;
    t_atom at[x->x_nchans];
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double xn = fabs(in[j*n + i]);
            if(x->x_reset){ // reset
                output = last_out[j] = xn;
                if(j == (x->x_nchans - 1))
                    x->x_reset = 0;
            }
            else{
                if(xn >= last_out[j])
                    a = ms_up > 0 ? exp(LOG001 / (ms_up * sr_khz)) : 0;
                else
                    a = ms_down > 0 ? exp(LOG001 / (ms_down * sr_khz)) : 0;
                if(a == 0)
                    output = yn = xn;
                else
                    output = yn = xn + a*(last_out[j] - xn);
                last_out[j] = yn;
            }
            if(i == n-1)
                SETFLOAT(&at[j], output);
        }
    }
    x->x_last_out = last_out;
    outlet_list(x->x_outlet, &s_list, x->x_nchans, at);
    return(w+4);
}

static void follow_dsp(t_follow *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    int chs = sp[0]->s_nchans, n = sp[0]->s_n;
    if(x->x_nchans != chs){
        x->x_last_out = (double *)resizebytes(x->x_last_out,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
    }
    if(x->x_float_mode)
        dsp_add(follow_perform_float, 3, x, n, sp[0]->s_vec);
    else{
        signal_setmultiout(&sp[1], x->x_nchans);
        dsp_add(follow_perform, 4, x, n, sp[0]->s_vec, sp[1]->s_vec);
    }
}

static void follow_reset(t_follow *x){
    x->x_reset = 1;
}
                        
static void *follow_free(t_follow *x){
    freebytes(x->x_last_out, x->x_nchans * sizeof(*x->x_last_out));
    return(void *)x;
}

static void *follow_new(t_symbol *s, int ac, t_atom * av){
    t_follow *x = (t_follow *)pd_new(follow_class);
    x->x_last_out = (double *)getbytes(sizeof(*x->x_last_out));
    x->x_reset = 0;
    x->x_nchans = 1;
    x->x_float_mode = 0;
    int argnum = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){ //if current argument is a float
            t_float aval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    x->x_ms_up  = aval;
                    break;
                case 1:
                    x->x_ms_down  = aval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--, av++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            if(atom_getsymbol(av) == gensym("-f")){
                x->x_float_mode = 1;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
    floatinlet_new(&x->x_obj, &x->x_ms_up);
    floatinlet_new(&x->x_obj, &x->x_ms_down);
    if(x->x_float_mode)
        x->x_outlet = outlet_new(&x->x_obj, gensym("list"));
    else
        outlet_new(&x->x_obj, gensym("signal"));
    return(x);
errstate:
    pd_error(x, "[follow~]: improper args");
    return(NULL);
}

void follow_tilde_setup(void){
    follow_class = class_new(gensym("follow~"), (t_newmethod)follow_new,
        (t_method)follow_free, sizeof(t_follow), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(follow_class, nullfn, gensym("signal"), 0);
    class_addmethod(follow_class, (t_method)follow_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(follow_class, (t_method)follow_reset, gensym("reset"), 0);
}
