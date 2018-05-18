// porres 2018

#include <math.h>
#include "m_pd.h"

typedef struct _drive{
    t_object  x_obj;
    t_inlet  *x_inlet;
}t_drive;

static t_class *drive_class;

static t_int *drive_perform(t_int *w){
    int n = (int)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    while(n--){
        t_float in = *in1++;
        t_float drive = *in2++;
        *out++ = tanhf(in * drive);
    }
    return(w+5);
}

static void drive_dsp(t_drive *x, t_signal **sp){
    dsp_add(drive_perform, 4, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

void *drive_new(t_symbol *s, int ac, t_atom *av){
    t_drive *x = (t_drive *)pd_new(drive_class);
    t_float drive = 1;
    if(ac && av->a_type == A_FLOAT)
        drive = atom_getfloatarg(0, ac, av);
    x->x_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet, drive);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void drive_tilde_setup(void){
    drive_class = class_new(gensym("drive~"), (t_newmethod)drive_new, 0,
            sizeof(t_drive), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(drive_class, nullfn, gensym("signal"), 0);
    class_addmethod(drive_class, (t_method) drive_dsp, gensym("dsp"), A_CANT, 0);
}
