// porres 2017-2024

#include <m_pd.h>
#include <buffer.h>

static t_class *xgate_class;

#define MAXOUTS 4096

typedef struct _xgate{
    t_object  x_obj;
    int       x_nchs;
    int       x_n;
    int       x_lastout;
    int       x_n_outs;
    double    x_n_fade; // fade length in samples
    float     x_sr_khz;
    int       x_active_out[MAXOUTS];
    int       x_count[MAXOUTS];
    t_float  *x_in;
    t_float **x_outs;
    t_outlet *x_out_status;
}t_xgate;

void xgate_float(t_xgate *x, t_floatarg f){
    int out = f < 0 ? 0 : f > x->x_n_outs ? x->x_n_outs : (int)f;
    if(x->x_lastout != out){
        if(out)
            x->x_active_out[out - 1] = 1;
        if(x->x_lastout)
            x->x_active_out[x->x_lastout - 1] = 0;
        x->x_lastout = out;
    }
}

static void xgate_time(t_xgate *x, t_floatarg f){
    double last_fade_n = x->x_n_fade;
    x->x_n_fade = (x->x_sr_khz * (f < 0 ? 0 : f)) + 1;
    for(int n = 0; n < x->x_n_outs; n++)
        if(x->x_count[n]) // adjust counters
            x->x_count[n] = (x->x_count[n] / last_fade_n) * x->x_n_fade;
}

static t_int *xgate_perform(t_int *w){
    t_xgate *x = (t_xgate *)(w[1]);
    for(int i = 0; i < x->x_n * x->x_nchs; i++){
        t_float input = x->x_in[i];
        for(int n = 0; n < x->x_n_outs; n++){
            if(x->x_active_out[n] && x->x_count[n] < x->x_n_fade)
                x->x_count[n]++;
            else if(!x->x_active_out[n] && x->x_count[n] > 0){
                x->x_count[n]--;
                if(x->x_count[n] == 0){
                    t_atom at[2];
                    SETFLOAT(at, n+1);
                    SETFLOAT(at+1, 0);
                    outlet_list(x->x_out_status, gensym("list"), 2, at);
                }
            }
            double amp = read_sintab(x->x_count[n] / x->x_n_fade * 0.25);
            x->x_outs[n][i] = input * amp;
        }
    }
    return(w+2);
}

static void xgate_dsp(t_xgate *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    x->x_nchs = sp[0]->s_nchans;
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    t_signal **sigp = sp;
    x->x_in = (*sigp++)->s_vec;             // input
    for(int i = 0; i < x->x_n_outs; i++){    // outlets
        signal_setmultiout(&sp[1+i], x->x_nchs);
        *(x->x_outs+i) = (*sigp++)->s_vec;
    }
    dsp_add(xgate_perform, 1, x);
}

void *xgate_free(t_xgate *x){
    freebytes(x->x_outs, x->x_n_outs * sizeof(*x->x_outs));
    return(void *)x;
}

static void *xgate_new(t_floatarg f1, t_floatarg f2, t_floatarg f3){
    t_xgate *x = (t_xgate *)pd_new(xgate_class);
    for(int n = 0; n < MAXOUTS; n++){
        x->x_active_out[n] = 0;
        x->x_count[n] = 0;
    }
    t_float out = f1, ms = f2, init_out = f3;
    x->x_n_outs = out < 1 ? 1 : out > MAXOUTS ? MAXOUTS : (int)out;
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_n_fade = x->x_sr_khz * (ms > 0 ? ms : 0) + 1;
    x->x_lastout = 0;
    x->x_outs = getbytes(x->x_n_outs * sizeof(*x->x_outs));
    for(int i = 0; i < x->x_n_outs; i++)
        outlet_new(&x->x_obj, gensym("signal"));
    x->x_out_status = outlet_new(&x->x_obj, &s_list);
    xgate_float(x, init_out);
    return(x);
}

void xgate_tilde_setup(void){
    xgate_class = class_new(gensym("xgate~"), (t_newmethod)xgate_new, (t_method)xgate_free,
        sizeof(t_xgate), CLASS_MULTICHANNEL, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addfloat(xgate_class, (t_method)xgate_float);
    class_addmethod(xgate_class, nullfn, gensym("signal"), 0);
    class_addmethod(xgate_class, (t_method)xgate_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(xgate_class, (t_method)xgate_time, gensym("time"), A_FLOAT, 0);
    init_sine_table();
}
