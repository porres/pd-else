// Porres 201

#include <math.h>
#include "m_pd.h"

#define ONESIXTH 0.1666666666666667

static t_class *lorenz_class;

typedef struct _lorenz{
    t_object  x_obj;
    int x_val;
    t_float x_sr;
    double  x_s;
    double  x_r;
    double  x_b;
    double  x_h;
    double  x_xnm1;
    double  x_ynm1;
    double  x_znm1;
    double  x_dx;
    double  x_phase;
    t_float  x_freq;
    t_outlet *x_outlet;
}t_lorenz;


static void lorenz_list(t_lorenz *x, t_symbol *s, int argc, t_atom * argv){
}

static t_int *lorenz_perform(t_int *w){
    t_lorenz *x = (t_lorenz *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    double s = x->x_s;
    double r = x->x_r;
    double b = x->x_b;
    double h = x->x_h;
    double xnm1 = x->x_xnm1;
    double ynm1 = x->x_ynm1;
    double znm1 = x->x_znm1;
    double phase = x->x_phase;
    double sr = x->x_sr;
    double dx = x->x_dx;
    double xn, yn, zn;
    /*  xnm1 = xn;
     ynm1 = yn;
     znm1 = zn; */
    while(nblock--){
        t_float hz = *in++;
        double phase_step = hz / sr; // phase_step
        phase_step = phase_step > 1 ? 1. : phase_step < -1 ? -1 : phase_step; // clipped phase_step
        int trig;
        t_float output;
        if(hz >= 0){
            trig = phase >= 1.;
            if (trig) phase = phase - 1;
        }
        else{
            trig = (phase <= 0.);
            if (trig) phase = phase + 1.;
        }
// update
        if(trig){
            double k1x, k2x, k3x, k4x,
            k1y, k2y, k3y, k4y,
            k1z, k2z, k3z, k4z,
            kxHalf, kyHalf, kzHalf;
            double hTimesS = h*s;
                
            double dx;

// 4th order Runge-Kutta
            k1x = hTimesS * (ynm1 - xnm1);
            k1y = h * (xnm1 * (r - znm1) - ynm1);
            k1z = h * (xnm1 * ynm1 - b * znm1);
            kxHalf = k1x * 0.5;
            kyHalf = k1y * 0.5;
            kzHalf = k1z * 0.5;
                
            k2x = hTimesS * (ynm1 + kyHalf - xnm1 - kxHalf);
            k2y = h * ((xnm1 + kxHalf) * (r - znm1 - kzHalf) - (ynm1 + kyHalf));
            k2z = h * ((xnm1 + kxHalf) * (ynm1 + kyHalf) - b * (znm1 + kzHalf));
            kxHalf = k2x * 0.5;
            kyHalf = k2y * 0.5;
            kzHalf = k2z * 0.5;
                
            k3x = hTimesS * (ynm1 + kyHalf - xnm1 - kxHalf);
            k3y = h * ((xnm1 + kxHalf) * (r - znm1 - kzHalf) - (ynm1 + kyHalf));
            k3z = h * ((xnm1 + kxHalf) * (ynm1 + kyHalf) - b * (znm1 + kzHalf));
                
            k4x = hTimesS * (ynm1 + k3y - xnm1 - k3x);
            k4y = h * ((xnm1 + k3x) * (r - znm1 - k3z) - (ynm1 + k3y));
            k4z = h * ((xnm1 + k3x) * (ynm1 + k3y) - b * (znm1 + k3z));
                
            xn = xnm1 + (k1x + 2.0*(k2x + k3x) + k4x) * ONESIXTH; // current
            yn = ynm1 + (k1y + 2.0*(k2y + k3y) + k4y) * ONESIXTH;
            zn = znm1 + (k1z + 2.0*(k2z + k3z) + k4z) * ONESIXTH;
                
            dx = xn - xnm1; // update delta
                
            xnm1 = xn;
            ynm1 = yn;
            znm1 = zn;
        }
        *out++ = (xnm1 + dx * phase) * 0.04;
        phase += phase_step;
    }
    x->x_phase = phase;
    x->x_xnm1 = xn;
    x->x_ynm1 = yn;
    x->x_znm1 = zn;
    x->x_dx = dx;
    return(w + 5);
}

static void lorenz_dsp(t_lorenz *x, t_signal **sp){
    x->x_sr = sp[0]->s_sr;
    dsp_add(lorenz_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void *lorenz_free(t_lorenz *x){
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *lorenz_new(t_symbol *s, int ac, t_atom *av){
    t_lorenz *x = (t_lorenz *)pd_new(lorenz_class);
    x->x_sr = sys_getsr();
    t_float nyq = x->x_sr * 0.5;
// default parameters
    t_float hz = nyq, s = 10, r = 28, b = ONESIXTH + 1, h = 0.05;
    t_float x = 0.1, y = 0, z = 0;
// default parameters
    if(hz >= 0) x->x_phase = 1;
    x->x_freq  = hz;
    x->x_s = s;
    x->x_r = r;
    x->x_b = b;
    x->x_x = x;
    x->x_y = y;
    x->x_z = z;
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void lorenz_tilde_setup(void){
    lorenz_class = class_new(gensym("lorenz~"), (t_newmethod)lorenz_new,
        (t_method)lorenz_free, sizeof(t_lorenz), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(lorenz_class, t_lorenz, x_freq);
    class_addlist(lorenz_class, lorenz_list);
    class_addmethod(lorenz_class, (t_method)lorenz_dsp, gensym("dsp"), A_CANT, 0);
}
