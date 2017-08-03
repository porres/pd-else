
#include "m_pd.h"
#include "math.h"

typedef struct _bcoeff
{
    t_object  x_obj;
    t_float   x_freq;
    t_float   x_q_s;
    t_float   x_gain;
} t_bcoeff;

static t_class *bcoeff_class;

static void bcoeff_bang(t_bcoeff *x)
{
    t_atom at[5];
/*    t_float omega = e_omega(x->x_freq,x->x_rate);
    t_float alpha = e_alpha(x->x_bw*0.01,omega);
    t_float b0 = 1 + alpha*e_A(x->x_gain);
    t_float b1 = -2.*cos(omega);
    t_float b2 = 1 - alpha*e_A(x->x_gain);
    t_float a0 = 1 + alpha/e_A(x->x_gain);
    t_float a1 = -2.*cos(omega);
    t_float a2 = 1 - alpha/e_A(x->x_gain);
    
    SETFLOAT(at,-a1/a0);
    SETFLOAT(at+1,-a2/a0);
    SETFLOAT(at+2,b0/a0);
    SETFLOAT(at+3,b1/a0);
    SETFLOAT(at+4,b2/a0);*/
    
    SETFLOAT(at, 0);
    SETFLOAT(at+1, 1);
    SETFLOAT(at+2, 2);
    SETFLOAT(at+3, 3);
    SETFLOAT(at+4, 4);
    
    outlet_list(x->x_obj.ob_outlet, &s_list, 5, at);
}

static void bcoeff_freq(t_bcoeff *x, t_floatarg val)
{
    x->x_freq = val;
    bcoeff_bang(x);
}

static void bcoeff_Q_S(t_bcoeff *x, t_floatarg val){
    x->x_q_s = val;
}

static void bcoeff_gain(t_bcoeff *x, t_floatarg val){
    x->x_gain = val;
}

static void *bcoeff_new(t_symbol *s, int argc, t_atom *argv){
    t_bcoeff *x = (t_bcoeff *)pd_new(bcoeff_class);
    x->x_freq = 0;
    inlet_new((t_object *)x, (t_pd *)x, &s_float, gensym("ft1"));
    inlet_new((t_object *)x, (t_pd *)x, &s_float, gensym("ft2"));
    outlet_new((t_object *)x, &s_list);
    return (x);
}

void bcoeff_setup(void)
{
    bcoeff_class = class_new(gensym("bcoeff"), (t_newmethod)bcoeff_new, 0,
			    sizeof(t_bcoeff), 0, A_GIMME, 0);
    class_addbang(bcoeff_class, bcoeff_bang);
    class_addfloat(bcoeff_class, bcoeff_freq);
    class_addmethod(bcoeff_class, (t_method)bcoeff_Q_S, gensym("ft1"), A_FLOAT, 0);
    class_addmethod(bcoeff_class, (t_method)bcoeff_gain, gensym("ft2"), A_FLOAT, 0);
}
