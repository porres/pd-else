
#include "m_pd.h"
#include <math.h>
#include "buffer.h"

static t_class *shaper_class;

typedef struct _shaper{
	t_object x_obj;
	t_buffer *x_buffer;
	t_outlet *x_out; //outlet
}t_shaper;

static double interppolate(t_word *buf, double i){ // linear interpolation
    int i1 = (int)i;
    int i2 = i1 + 1;
    double frac = i - (double)i1;
    double ya = buf[i1].w_float;
    double yb = buf[i2].w_float;
    double interpolation = ya + ((yb - ya) * frac);
    return interpolation;
}

static void shaper_set(t_shaper *x, t_symbol *s){
   buffer_setarray(x->x_buffer, s);
}

static t_int *shaper_perform(t_int *w){
	t_shaper *x = (t_shaper *)(w[1]);
	t_float *in = (t_float *)(w[2]);
	t_float *out = (t_float *)(w[3]);
    int n = (int)(w[4]);
    t_word *buf = (t_word *)x->x_buffer->c_vectors[0];
	double maxidx = (double)(x->x_buffer->c_npts - 1);
	while(n--){
		double output = 0; // silence if no buffer
        if(x->x_buffer->c_playable){ // read the buffer and interpolate
            double ph = ((double)*in++ + 1) * 0.5; // get phase (0-1)
            while(ph < 0) // wrap
                ph++;
            while(ph >= 1)
                ph--;
            double i = ph * maxidx;
            output = interppolate(buf, i);
        }
		*out++ = output;
	};
	return(w+5);
}

static void shaper_dsp(t_shaper *x, t_signal **sp){
    buffer_checkdsp(x->x_buffer);	
    dsp_add(shaper_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

static void *shaper_free(t_shaper *x){
    outlet_free(x->x_out);
	return (void *)x;
}

static void *shaper_new(t_symbol *s, int ac, t_atom *av){
    t_shaper *x = (t_shaper *)pd_new(shaper_class);
    t_symbol *name = s; // get rid of warning
    name = NULL;
    if(ac == 1 && av->a_type == A_SYMBOL)
        name = atom_getsymbolarg(0, ac, av);
    x->x_buffer = buffer_init((t_class *) x, name, 1, 0);
    x->x_out = outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void shaper_tilde_setup(void){
   shaper_class = class_new(gensym("shaper~"), (t_newmethod)shaper_new,
        (t_method)shaper_free, sizeof(t_shaper), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(shaper_class, nullfn, gensym("signal"), 0);
    class_addmethod(shaper_class, (t_method)shaper_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(shaper_class, (t_method)shaper_set, gensym("set"), A_SYMBOL, 0);
}
