// porres based on cyclone/wave~

#define _USE_MATH_DEFINES
#include <math.h>
#include "m_pd.h"
#include "shared/buffer.h"

typedef struct _table2{
    t_object  x_obj;
    t_buffer *x_buffer;
    int       x_bufsize;
    int       x_interp_mode;
    int       x_numouts; // number of outputs
    t_float   x_f;       // dummy
    t_float	  x_bias;
    t_float   x_tension;
    t_float  *x_in;      // inlet vector
    t_float **x_ovecs;   // output vectors
}t_table2;

static t_class *table2_class;

static void table2_interp(t_table2 *x, t_floatarg f){
	x->x_interp_mode = (int)f;
    buffer_setminsize(x->x_buffer, x->x_interp_mode > 0 ? 4 : 1);
    buffer_playcheck(x->x_buffer);
}

static void table2_set_linear(t_table2 *x){
    table2_interp(x, 0);
}

static void table2_set_cubic(t_table2 *x){
    table2_interp(x, 1);
}

static void table2_set_spline(t_table2 *x){
    table2_interp(x, 2);
}

static void table2_set_hermite(t_table2 *x, t_floatarg bias, t_floatarg tension){
    x->x_bias = bias;
    x->x_tension = tension;
    table2_interp(x, 3);
}

static void table2_set_lagrange(t_table2 *x){
    table2_interp(x, 4);
}

static void table2_set(t_table2 *x, t_symbol *s){
    buffer_setarray(x->x_buffer, s);
}

// stupid hacks that saves lots of typing
#define BOUNDS_CHECK() \
	if(spos < 0) spos = 0; \
	else if(spos > maxindex) spos = maxindex; \
	if(epos > maxindex || epos <= 0) epos = maxindex; \
	else if(epos < spos) epos = spos; \
	int siz = (int)(epos - spos + 1.5); \
	int ndx; \
	int ch = nch; \
	if(phase < 0) phase = 0; \
	else if(phase > 1.0) phase = 0; \
	int sposi = (int)spos; \
	int eposi = sposi + siz
	
#define INDEX_2PT(TYPE) \
	int ndx1; \
	TYPE a, b; \
	TYPE xpos = phase*siz + spos; \
	ndx = (int)xpos; \
	TYPE frac = xpos - ndx; \
	if(ndx == eposi) ndx = sposi; \
	ndx1 = ndx + 1; \
	if(ndx1 == eposi) ndx1 = sposi
	
#define INDEX_4PT() \
	int ndxm1, ndx1, ndx2; \
	double a, b, c, d; \
	double xpos = phase*siz + spos; \
	ndx = (int)xpos; \
	double frac = xpos - ndx; \
	if(ndx == eposi) ndx = sposi; \
	ndxm1 = ndx - 1; \
	if(ndxm1 < sposi) ndxm1 = eposi - 1; \
	ndx1 = ndx + 1; \
	if(ndx1 == eposi) ndx1 = sposi; \
	ndx2 = ndx1 + 1; \
	if(ndx2 == eposi) ndx2 = sposi;

static void table2_linear(t_table2 *x, t_int *outp, t_float *xin, int nblock, int nch, int maxindex, t_word **vectable){
    x = NULL;
	int iblock;
	for(iblock = 0; iblock < nblock; iblock++){
		double phase = (double)(*xin++);
		double spos = 0;
		double epos = 0;
		BOUNDS_CHECK();
		INDEX_2PT(double);
		while(ch--){
			t_word *vp = vectable[ch];
			t_float *out = (t_float *)(outp[ch]);
			if(vp){
				a = (double)vp[ndx].w_float;
				b = (double)vp[ndx1].w_float;
				out[iblock] = (t_float)(a * (1.0 - frac) + b * frac);
			}
			else
                out[iblock] = 0;
		}
	}
	return;
}

static void table2_cubic(t_table2 *x, t_int *outp, t_float *xin,
int nblock, int nch, int maxindex, t_word **vectable){
    x = NULL;
	for(int iblock = 0; iblock < nblock; iblock++){
		t_float phase = *xin++;
		t_float spos = 0;
		t_float epos = 0;
		BOUNDS_CHECK();
		INDEX_4PT();
		while(ch--){
			t_word *vp = vectable[ch];
			t_float *out = (t_float *)(outp[ch]);
			if(vp){
				a = (double)vp[ndxm1].w_float;
				b = (double)vp[ndx].w_float;
				c = (double)vp[ndx1].w_float;
				d = (double)vp[ndx2].w_float;
				double p0, p1, p2;
				p0 = d - a + b - c;
				p1 = a - b - p0;
				p2 = c - a;
				out[iblock] = (t_float)(b+frac*(p2+frac*(p1+frac*p0)));	
			}
			else
                out[iblock] = 0;
		}
	}
	return;
}

static void table2_spline(t_table2 *x, t_int *outp, t_float *xin,
int nblock, int nch, int maxindex, t_word **vectable){
    x = NULL;
	int iblock;
	for(iblock = 0; iblock < nblock; iblock++){
		t_float phase = *xin++;
		t_float spos = 0;
		t_float epos = 0;
		BOUNDS_CHECK();
		INDEX_4PT();
		while(ch--){
			t_word *vp = vectable[ch];
			t_float *out = (t_float *)(outp[ch]);
			if(vp){
				a = (double)vp[ndxm1].w_float;
				b = (double)vp[ndx].w_float;
				c = (double)vp[ndx1].w_float;
				d = (double)vp[ndx2].w_float;
				double p0, p1, p2;
				p0 = 0.5*(d - a) + 1.5*(b - c);
				p2 = 0.5*(c - a);
				p1 = a - b + p2 - p0;
				out[iblock] = (t_float)(b+frac*(p2+frac*(p1+frac*p0)));	
			}
			else
                out[iblock] = 0;
		}
		
	}
	return;
}

static void table2_hermite(t_table2 *x, t_int *outp, t_float *xin,
int nblock, int nch, int maxindex, t_word **vectable){
	int iblock;
	for(iblock = 0; iblock < nblock; iblock++){
		t_float phase = *xin++;
		t_float spos = 0;
		t_float epos = 0;
		BOUNDS_CHECK();
		INDEX_4PT();
		double tension = (double)x->x_tension;
		double bias = (double)x->x_bias;
		while(ch--){
			t_word *vp = vectable[ch];
			t_float *out = (t_float *)(outp[ch]);
			if(vp){
				a = (double)vp[ndxm1].w_float;
				b = (double)vp[ndx].w_float;
				c = (double)vp[ndx1].w_float;
				d = (double)vp[ndx2].w_float;
				double p0, p1, p2, p3, m0, m1;
				double frac2 = frac*frac;
				double frac3 = frac*frac2;
				double cminusb = c - b;
				double bias1 = 1. - bias;
				bias++;
				tension = 0.5 * (1. - tension);
				m0 = tension * ((b-a)*bias + cminusb*bias1);
				m1 = tension * (cminusb*bias + (d-c)*bias1);
				p2 = frac3 - frac2;
				p0 = 2*p2 - frac2 + 1.;
				p1 = p2 - frac2 + frac;
				p3 = frac2 - 2*p2;
				out[iblock] = (t_float)(p0*b + p1*m0 + p2*m1 + p3*c);	
			}
			else out[iblock] = 0;
		}
	}
	return;
}

static void table2_lagrange(t_table2 *x, t_int *outp, t_float *xin,
int nblock, int nch, int maxindex, t_word **vectable){
    x = NULL;
	int iblock;
	for(iblock = 0; iblock < nblock; iblock++){
		t_float phase = *xin++;
		t_float spos = 0;
		t_float epos = 0;
		BOUNDS_CHECK();
		INDEX_4PT();
		while(ch--){
			t_word *vp = vectable[ch];
			t_float *out = (t_float *)(outp[ch]);
			if(vp){
				a = (double)vp[ndxm1].w_float;
				b = (double)vp[ndx].w_float;
				c = (double)vp[ndx1].w_float;
				d = (double)vp[ndx2].w_float;
				double cminusb = c-b;
				out[iblock] = (t_float)(b + frac * (
					cminusb - (1. - frac)/6. * (
						(d - a - 3.0*cminusb) * frac + d + 2.0*a - 3.0*b
					)
				));	
			}
			else
                out[iblock] = 0;
		}
	}
	return;
}

static t_int *table2_perform(t_int *w){
	static void (* const wif[])(t_table2 *x, t_int *outp, t_float *xin, int nblock, int nch,
    int maxindex, t_word **vectable) ={ // jump table for interpolation functions
   		table2_linear, table2_cubic, table2_spline, table2_hermite, table2_lagrange
    };
    t_table2 *x = (t_table2 *)(w[1]);
    int nblock = (int)(w[2]);
    t_buffer * c = x->x_buffer;
    t_int *outp = (t_int *)x->x_ovecs;
    int nch = c->c_numchans;
    if(c->c_playable) // call interpolation function (also performs block loop)
		wif[x->x_interp_mode](x, outp, x->x_in, nblock, nch, c->c_npts - 1, c->c_vectors);
    else{
        int ch = nch;
        while(ch--){
            t_float *out = (t_float *)outp[ch];
            while(nblock--)
                *out++ = 0;
        }
    }
    return(w+3);
}

static void table2_dsp(t_table2 *x, t_signal **sp){
    buffer_checkdsp(x->x_buffer);
    t_signal **sigp = sp;
	x->x_in = (*sigp++)->s_vec; // the first sig in is the input
    for(int i = 0; i < x->x_numouts; i++) // now for the outputs
		*(x->x_ovecs+i) = (*sigp++)->s_vec;
	dsp_add(table2_perform, 2, x, sp[0]->s_n);
}

static void table2_free(t_table2 *x){
    buffer_free(x->x_buffer);
    freebytes(x->x_ovecs, x->x_numouts * sizeof(*x->x_ovecs));
}

static void *table2_new(t_symbol *s, int ac, t_atom * av){
    s = NULL;
    t_table2 *x = (t_table2 *)pd_new(table2_class);
	//mostly copying this for what i did with record~ - DXK
	t_symbol * name = NULL;
	int nameset = 0; //flag if name is set
	//setting defaults
    int numouts = 1; //i'm assuming the default is 1 - DXK
	t_float bias = 0;
	t_float tension = 0;
	t_float interp = 7;
	while(ac){
		if(av->a_type == A_SYMBOL){ // symbol
            t_symbol * curarg = atom_getsymbolarg(0, ac, av);
            if(curarg == gensym("-cubic")){
                if(nameset)
                    goto errstate;
                interp = 4;
                ac--, av++;
            }
            else if(curarg == gensym("-linear")){
                if(nameset)
                    goto errstate;
                interp = 1;
                ac--, av++;
            }
            else if(curarg == gensym("-spline")){
                if(nameset)
                    goto errstate;
                interp = 5;
                ac--, av++;
            }
            else if(curarg == gensym("-hermite")){
                if(nameset)
                    goto errstate;
                if(ac >= 3){
                    interp = 6;
                    ac--, av++;
                    bias = atom_getfloat(av);
                    ac--, av++;
                    tension = atom_getfloat(av);
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else{
                if(nameset)
                    goto errstate;
                name = atom_getsymbolarg(0, ac, av);
                nameset = 1; //set nameset flag
                ac--, av++;
            }
        }
		else{ // float
            numouts = (int)atom_getfloatarg(0, ac, av);
            ac--, av++;
        };
	};
    //some boundschecking
	if(numouts > 64)
		numouts = 64;
	else if(numouts < 1)
		numouts = 1;
    x->x_buffer = buffer_init((t_class *)x, name, numouts, 0);
    x->x_numouts = numouts;
    // allocating output vectors
    x->x_ovecs = getbytes(x->x_numouts * sizeof(*x->x_ovecs));
	table2_interp(x, 4);
	x->x_bias = bias;
	x->x_tension = tension;
	for(int i = 0; i < numouts; i++)
		outlet_new(&x->x_obj, gensym("signal"));
	return(x);
	errstate:
		post("table2~: improper args");
		return(NULL);
}

void table2_tilde_setup(void){
    table2_class = class_new(gensym("table2~"), (t_newmethod)table2_new,
        (t_method)table2_free, sizeof(t_table2), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(table2_class, t_table2, x_f);
    class_addmethod(table2_class, (t_method)table2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(table2_class, (t_method)table2_set, gensym("set"), A_SYMBOL, 0);
    class_addmethod(table2_class, (t_method)table2_set_lagrange, gensym("lagrange"), 0);
    class_addmethod(table2_class, (t_method)table2_set_cubic, gensym("cubic"), 0);
    class_addmethod(table2_class, (t_method)table2_set_linear, gensym("linear"), 0);
    class_addmethod(table2_class, (t_method)table2_set_spline, gensym("spline"), 0);
    class_addmethod(table2_class, (t_method)table2_set_hermite, gensym("hermite"),
        A_FLOAT, A_FLOAT, 0);
}
