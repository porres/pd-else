// porres 2018

#include <math.h>
#include <m_pd.h>
#include <stdlib.h>

typedef struct _drive{
    t_object  x_obj;
    t_inlet  *x_inlet;
    t_int     x_mode;
    t_int     x_sig2;
    t_int     x_ch1;
    t_int     x_ch2;
    t_int     x_n;
    int       x_nchans;
}t_drive;

static t_class *drive_class;

static void drive_mode(t_drive *x, t_floatarg f){
    x->x_mode = f < 0 ? 0 : f > 2 ? 2 : (int)f;
}

static t_int *drive_perform(t_int *w){
    t_drive *x = (t_drive *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    t_float mode = x->x_mode;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            float in = x->x_ch1 == 1 ? in1[i] : in1[j*n + i];
            float f2 = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            if(f2 < 0)
                f2 = 0;
            if(mode == 0)
                out[j*n + i] = tanhf(in * f2);
            else if(mode == 1){
                if(in >= 1.)
                    out[j*n + i] = 1.;
                else if(in <= -1)
                    out[j*n + i] = -1.;
                else if(f2 < 1)
                    out[j*n + i] = f2 * in;
                else if(in > 0)
                    out[j*n + i] = 1. - powf(1. - in, f2);
                else
                    out[j*n + i] = powf(1. + in, f2) - 1.;
            }
            else{
                if(f2 > 1)
                    f2 = 1;
                t_float abs_in = fabs(in);
                out[j*n + i] = abs_in > f2 ? copysignf((1-(f2*(f2-2)+1) / (abs_in-2*f2+1)), in) : in;
            }
        }
    }
    return(w+5);
}

static void drive_dsp(t_drive *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    x->x_nchans = x->x_ch1 = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    if(x->x_ch2 > x->x_nchans)
        x->x_nchans = x->x_ch2;
    signal_setmultiout(&sp[2], x->x_nchans);
    if((x->x_ch1 > 1 && x->x_ch1 != x->x_nchans)
    || (x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[drive~]: channel sizes mismatch");
        return;
    }
    dsp_add(drive_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

void *drive_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_drive *x = (t_drive *)pd_new(drive_class);
    t_float drive = 1;
    x->x_mode = 0;
    int arg = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            drive = atom_getfloatarg(0, ac, av);
            ac--, av++;
            arg = 1;
        }
        else if(av->a_type == A_SYMBOL && !arg && ac >= 2){
            if(atom_getsymbolarg(0, ac, av) == gensym("-mode")){
                ac--, av++;
                if(av->a_type == A_FLOAT){
                    t_float f = atom_getfloatarg(0, ac, av);
                    x->x_mode = f < 0 ? 0 : f > 2 ? 2 : (int)f;
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    x->x_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet, drive);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
errstate:
    pd_error(x, "[drive~]: improper args");
    return NULL;
}

void drive_tilde_setup(void){
    drive_class = class_new(gensym("drive~"), (t_newmethod)drive_new, 0,
        sizeof(t_drive), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(drive_class, nullfn, gensym("signal"), 0);
    class_addmethod(drive_class, (t_method) drive_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(drive_class, (t_method)drive_mode, gensym("mode"), A_DEFFLOAT, 0);
}
