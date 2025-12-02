// Porres 2018-2023

#include <m_pd.h>
#include <math.h>

static t_class *quantizer_class;

typedef struct _quantizer{
	t_object    x_obj;
    t_inlet    *x_inlet_f;
	int         x_mode;
    int         x_nchans;
    t_int       x_n;
    t_int       x_ch1;
    t_int       x_ch2;
}t_quantizer;

static void quantizer_mode(t_quantizer *x, t_float f){
    x->x_mode = (int)f;
    if(x->x_mode < 0)
        x->x_mode = 0;
    if(x->x_mode > 4)
        x->x_mode = 4;
}

static t_int *quantizer_perform(t_int *w){
	t_quantizer *x = (t_quantizer *)(w[1]);
    t_sample *in1 = (t_sample *)(w[2]);
    t_sample *in2 = (t_sample *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float in = x->x_ch1 == 1 ? in1[i] : in1[j*n + i];
            float step = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            float output;
            if(step > 0.){ // quantize
                float div = in/step; // get division
                if(x->x_mode == 0) // round
                    output = step * round(div);
                else if(x->x_mode == 1) // truncate
                    output = step * trunc(div);
                else if(x->x_mode == 2) // floor
                    output = step * floor(div);
                else if(x->x_mode == 3) // ceil
                    output = step * ceil(div);
                else{ // floor & ceil (mode == 4)
                    if(in > 0)
                        output = step * ceil(div);
                    else
                        output = step * floor(div);
                }
            }
            else // step is <= 0, do nothing
                output = in;
            out[j*n + i] = output;
        }
    };
	return(w+5);
}

static void quantizer_dsp(t_quantizer *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_ch1 = sp[0]->s_nchans, x->x_ch2 = sp[1]->s_nchans;
    x->x_nchans = x->x_ch1 > x->x_ch2 ? x->x_ch1 : x->x_ch2;
    signal_setmultiout(&sp[2], x->x_nchans);
    if(x->x_ch1 > 1 && x->x_ch1 != x->x_nchans ||
    x->x_ch2 > 1 && x->x_ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[quantizer~]: channel sizes mismatch");
        return;
    }
    dsp_add(quantizer_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *quantizer_free(t_quantizer *x){
    inlet_free(x->x_inlet_f);
    return(void *)x;
}

static void *quantizer_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_quantizer *x = (t_quantizer *)pd_new(quantizer_class);
    float f = 0;
    int numargs = 0;
    x->x_mode = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            switch(numargs){
                case 0:
                    f = atom_getfloat(av);
                    ac--, av++;
                    break;
                case 1:
                    x->x_mode = atom_getint(av);
                    ac--, av++;
                    break;
                default:
                    ac--, av++;
                    break;
            };
            numargs++;
        }
        else if(av->a_type == A_SYMBOL && !numargs){
            if(atom_getsymbolarg(0, ac, av) == gensym("-mode") && ac >= 2){
                if((av+1)->a_type == A_FLOAT){
                    x->x_mode = (int)atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
    if(x->x_mode < 0)
        x->x_mode = 0;
    if(x->x_mode > 4)
        x->x_mode = 4;
    x->x_inlet_f = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_f, f);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
errstate:
    pd_error(x, "[quantizer~]: improper args");
    return(NULL);
}

void quantizer_tilde_setup(void){
	quantizer_class = class_new(gensym("quantizer~"), (t_newmethod)quantizer_new, (t_method)quantizer_free, sizeof(t_quantizer), CLASS_MULTICHANNEL, A_GIMME, 0);
	class_addmethod(quantizer_class, nullfn, gensym("signal"), 0);
	class_addmethod(quantizer_class, (t_method)quantizer_dsp, gensym("dsp"), A_CANT, 0);
	class_addmethod(quantizer_class, (t_method)quantizer_mode,  gensym("mode"), A_FLOAT, 0);
}
