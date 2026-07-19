// porres 2017-2024

#include <m_pd.h>
#include <buffer.h>

static t_class *xselect_class;

#define INPUTLIMIT 4096

typedef struct _xselect{
    t_object    x_obj;
    int         x_nchs;
    int         x_n;
    int         x_channel;
    int         x_lastchannel;
    int         x_ninlets;
    double      x_fade_in_samps;
    float       x_sr_khz;
    int         x_active_channel[INPUTLIMIT];
    int         x_counter[INPUTLIMIT];
    double      x_fade[INPUTLIMIT];
    t_float   **x_ins;
    t_float    *x_out;
    t_outlet   *x_out_status;
}t_xselect;

void xselect_float(t_xselect *x, t_floatarg ch){
  ch = (int)ch > x->x_ninlets ? x->x_ninlets : (int)ch;
  x->x_channel = ch < 0 ? 0 : (int)ch;
  if(x->x_channel != x->x_lastchannel){
      if(x->x_channel)
          x->x_active_channel[x->x_channel - 1] = 1;
      if(x->x_lastchannel)
          x->x_active_channel[x->x_lastchannel - 1] = 0;
      x->x_lastchannel = x->x_channel;
  }
}

static t_int *xselect_perform(t_int *w){
    t_xselect *x = (t_xselect *)(w[1]);
    int n = x->x_n, chs = x->x_nchs;
    for(int i = 0; i < n; i++){
        for(int in = 0; in < x->x_ninlets; in++){
            if(x->x_active_channel[in] && x->x_counter[in] < x->x_fade_in_samps)
                x->x_counter[in]++;
            else if(!x->x_active_channel[in] && x->x_counter[in] > 0){
                x->x_counter[in]--;
                if(x->x_counter[in] == 0){
                    t_atom at[2];
                    SETFLOAT(at, in + 1);
                    SETFLOAT(at + 1, 0);
                    outlet_list(x->x_out_status, gensym("list"), 2, at);
                }
            }
            x->x_fade[in] = x->x_fade_in_samps ? x->x_counter[in] / x->x_fade_in_samps : 1;
            x->x_fade[in] = read_sintab(x->x_fade[in] * 0.25);
        }
        for(int j = 0; j < chs; j++){
            double sum = 0;
            for(int in = 0; in < x->x_ninlets; in++)
                sum += x->x_ins[in][j*n + i] * x->x_fade[in];
            x->x_out[j*n + i] = sum;
        }
    }
    return(w+2);
}

static void xselect_dsp(t_xselect *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    t_signal **sigp = sp;
    x->x_nchs = sp[0]->s_nchans;
    signal_setmultiout(&sp[x->x_ninlets], x->x_nchs);
    for(int i = 0; i < x->x_ninlets; i++){
        if(sp[i]->s_nchans != x->x_nchs){
            post("[xselect~]: multichannel inputs don't match");
            dsp_add_zero(sp[x->x_ninlets]->s_vec, x->x_nchs * x->x_n);
            return;
        }
        x->x_ins[i] = (*sigp++)->s_vec;
    }
    x->x_out = (*sigp)->s_vec;
    dsp_add(xselect_perform, 1, x);
}

static void xselect_time(t_xselect *x, t_floatarg ms){
    double last_fade_in_samps = x->x_fade_in_samps;
    ms = ms < 0 ? 0 : ms;
    x->x_fade_in_samps = x->x_sr_khz * ms;
    for(int i = 0; i < x->x_ninlets; i++){
        if(x->x_counter[i] && last_fade_in_samps)
            x->x_counter[i] =
                x->x_counter[i] / last_fade_in_samps * x->x_fade_in_samps;
    }
}

void *xselect_free(t_xselect *x){
    freebytes(x->x_ins, x->x_ninlets * sizeof(*x->x_ins));
    return(void *)x;
}

static void *xselect_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_xselect *x = (t_xselect *)pd_new(xselect_class);
    x->x_sr_khz = sys_getsr() * 0.001;
    t_float ch = 1, ms = 0, init_channel = 0;
    int i;
    int argnum = 0;
    while(ac > 0){
        if(av -> a_type == A_FLOAT){ // if current argument is a float
            t_float aval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    ch = aval;
                    break;
                case 1:
                    ms = aval;
                    break;
                case 2:
                    init_channel = aval;
                default:
                    break;
            };
        };
        argnum++;
        ac--;
        av++;
    };
    x->x_ninlets = ch < 1 ? 1 : ch;
    if(x->x_ninlets > INPUTLIMIT)
        x->x_ninlets = INPUTLIMIT;
    x->x_ins = getbytes(x->x_ninlets * sizeof(*x->x_ins));
    for(i = 0; i < x->x_ninlets - 1; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    x->x_out_status = outlet_new(&x->x_obj, &s_list);
    ms = ms > 0 ? ms : 0;
    x->x_fade_in_samps = x->x_sr_khz * ms + 1;
    x->x_lastchannel = 0;
    for(i = 0; i < INPUTLIMIT; i++){
        x->x_active_channel[i] = 0;
        x->x_counter[i] = 0;
        x->x_fade[i] = 0;
    }
    xselect_float(x, init_channel);
    return(x);
}

void xselect_tilde_setup(void){
    xselect_class = class_new(gensym("xselect~"), (t_newmethod)xselect_new,
        (t_method)xselect_free, sizeof(t_xselect), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addfloat(xselect_class, (t_method)xselect_float);
    class_addmethod(xselect_class, nullfn, gensym("signal"), 0);
    class_addmethod(xselect_class, (t_method)xselect_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(xselect_class, (t_method)xselect_time, gensym("time"), A_FLOAT, 0);
    init_sine_table();
}
