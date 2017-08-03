
#include "m_pd.h"
#include "math.h"

#define PI M_PI

typedef struct _bcoeff{
    t_object  x_obj;
    t_float   x_freq;
    t_float   x_q_s;
    t_float   x_gain;
} t_bcoeff;

static t_class *bcoeff_class;

static void bcoeff_bang(t_bcoeff *x)
{
    t_atom at[5];
    double omega, alphaQ, cos_w, a0, a1, a2, b0, b1, b2;
    double hz = (double)x->x_freq;
    double q = (double)x->x_q_s;
    double amp = pow(10, x->x_gain / 40);
    double nyq = sys_getsr() * 0.5;
    if (hz < 0.1)
        hz = 0.1;
    if (hz > nyq - 0.1)
        hz = nyq - 0.1;
    if (q < 0.000001)
        q = 0.000001; // prevent blow-up
    
    omega = hz * PI/nyq;
    alphaQ = sin(omega) / (2*q);
    cos_w = cos(omega);
    b0 = alphaQ/amp + 1;
    a0 = (1 + alphaQ*amp) / b0;
    a1 = -2*cos_w / b0;
    a2 = (1 - alphaQ*amp) / b0;
    b1 = 2*cos_w / b0;
    b2 = (alphaQ/amp - 1) / b0;
    
    SETFLOAT(at, b1);
    SETFLOAT(at+1, b2);
    SETFLOAT(at+2, a0);
    SETFLOAT(at+3, a1);
    SETFLOAT(at+4, a2);
    
    outlet_list(x->x_obj.ob_outlet, &s_list, 5, at);
}

static void bcoeff_freq(t_bcoeff *x, t_floatarg val)
{
    x->x_freq = val;
    bcoeff_bang(x);
}

static void bcoeff_Q_S(t_bcoeff *x, t_floatarg val){
    x->x_q_s = val;
    bcoeff_bang(x);
}

static void bcoeff_gain(t_bcoeff *x, t_floatarg val){
    x->x_gain = val;
    bcoeff_bang(x);
}

static void *bcoeff_new(t_symbol *s, int argc, t_atom *argv){
    t_bcoeff *x = (t_bcoeff *)pd_new(bcoeff_class);
    x->x_freq = 0;
    inlet_new((t_object *)x, (t_pd *)x, &s_float, gensym("ft1"));
    inlet_new((t_object *)x, (t_pd *)x, &s_float, gensym("ft2"));
    outlet_new((t_object *)x, &s_list);
    return (x);
}

void bcoeff_setup(void){
    bcoeff_class = class_new(gensym("bcoeff"), (t_newmethod)bcoeff_new, 0,
			    sizeof(t_bcoeff), 0, A_GIMME, 0);
    class_addbang(bcoeff_class, bcoeff_bang);
    class_addfloat(bcoeff_class, bcoeff_freq);
    class_addmethod(bcoeff_class, (t_method)bcoeff_Q_S, gensym("ft1"), A_FLOAT, 0);
    class_addmethod(bcoeff_class, (t_method)bcoeff_gain, gensym("ft2"), A_FLOAT, 0);
}
