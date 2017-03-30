// Porres 2016

#include "m_pd.h"
#include <math.h>

static t_class *cents2ratio_class;

typedef struct _cents2ratio {
  t_object x_obj;
  t_inlet *x_inlet;
  t_outlet *x_outlet;
} t_cents2ratio;

void *cents2ratio_new(void);
static t_int * cents2ratio_perform(t_int *w);
static void cents2ratio_dsp(t_cents2ratio *x, t_signal **sp);

static t_int * cents2ratio_perform(t_int *w)
{
  t_cents2ratio *x = (t_cents2ratio *)(w[1]); // ???
  int n = (int)(w[2]);
  t_float *in = (t_float *)(w[3]);
  t_float *out = (t_float *)(w[4]);
  while(n--)
*out++ = pow(2, (*in++/1200));
  return (w + 5);
}

A non-interpolating sound generator based on the difference equations:
x(n) = 1 - y[n-1] + abs(x[n-1])
y(n) = x(n-1)


////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GbmanN_next(GbmanN *unit, int inNumSamples){
    float *out = ZOUT(0);
    float freq = ZIN0(0);
    
    double last_out = unit->last_out;
    double y_nm2 = unit->y_nm2;
    float counter = unit->counter;
    
    float samplesPerCycle;
    if(freq < unit->mRate->mSampleRate)
        samplesPerCycle = unit->mRate->mSampleRate / sc_max(freq, 0.001f);
    else samplesPerCycle = 1.f;
    
    for (int i=0; i<inNumSamples; ++i) {
        if(counter >= samplesPerCycle){
            counter -= samplesPerCycle;
            double y_nm1 = last_out;

            double out = 1.f - y_nm2 + abs(y_nm1);
            
            y_nm2 = y_nm1;
        }
        counter++;
        ZXP(out) = out;
    }
    unit->last_out = out;
    unit->y_nm2 = y_nm2;
    unit->counter = counter;
}

void GbmanN_Ctor(GbmanN *unit){
    SETCALC(GbmanN_next);
    unit->last_out = ZIN0(1);
    unit->y_nm2 = ZIN0(2);
    unit->counter = 0.f;
    GbmanN_next(unit, 1);
}

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////


static void cents2ratio_dsp(t_cents2ratio *x, t_signal **sp)
{
  dsp_add(cents2ratio_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

void *cents2ratio_new(void)
{
  t_cents2ratio *x = (t_cents2ratio *)pd_new(cents2ratio_class); 
  x->x_inlet = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
  x->x_outlet = outlet_new(&x->x_obj, &s_signal);
  return (void *)x;
}

void cents2ratio_tilde_setup(void) {
  cents2ratio_class = class_new(gensym("cents2ratio~"),
    (t_newmethod) cents2ratio_new, 0, sizeof (t_cents2ratio), CLASS_NOINLET, 0);
  class_addmethod(cents2ratio_class, (t_method) cents2ratio_dsp, gensym("dsp"), 0);
}
