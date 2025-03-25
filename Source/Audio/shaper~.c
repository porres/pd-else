// porres

#include <m_pd.h>
#include <buffer.h>
#include <math.h>
#include <stdlib.h>

#define FLEN      65536
#define MAX_COEF  256

static t_class *shaper_class;

typedef struct _shaper{
    t_object    x_obj;
    t_float    *x_cheby;
    t_float    *x_coef;
    t_int       x_count;
    t_int       x_norm;
    t_int       x_arrayset;
    t_int       x_dc_filter;
    t_float     x_sr;
    double      x_a;
    double      x_xnm1;
    double      x_ynm1;
    t_buffer   *x_buffer;
}t_shaper;

static void shaper_set(t_shaper *x, t_symbol *s){
    buffer_setarray(x->x_buffer, s);
    x->x_arrayset = 1;
}

static void update_cheby_func(t_shaper *x){
    int i;
    for(i = 0; i < FLEN; i++) // clear
        x->x_cheby[i] = 0;
    for(i = 0 ; i < x->x_count; i++){
        if(x->x_coef[i] > 0.0){
            for(int j = 0; j < FLEN; j++){
                float p = -1.0 + 2.0 * ((float)j / (float)FLEN);
                x->x_cheby[j] += (x->x_coef[i] * cos((float)i * acos(p)));
            }
        }
    }
    if(x->x_norm){ // normalize
        float  min = 1, max = -1; // find min/max
        for(i = 0; i < FLEN; i++){
            if(x->x_cheby[i] < min)
                min = x->x_cheby[i];
            if(x->x_cheby[i] > max)
                max = x->x_cheby[i];
        }
        for(i = 0; i < FLEN; i++){
            if((max - min) == 0)
                x->x_cheby[i] = x->x_coef[0] != 0;
            else
                x->x_cheby[i] = 2.0 * ((x->x_cheby[i] - min) / (max - min)) - 1.0;
        }
    }
}

static void shaper_filter(t_shaper *x, t_float f){
    x->x_dc_filter = f != 0;
}

static void shaper_dc(t_shaper *x, t_float f){
    x->x_coef[0] = f;
    if(!x->x_norm)
        update_cheby_func(x);
    x->x_arrayset = 0;
}

static void shaper_norm(t_shaper *x, t_float f){
    x->x_norm = f != 0;
    update_cheby_func(x);
    x->x_arrayset = 0;
}

static void shaper_list(t_shaper *x, t_symbol *s, short ac, t_atom *av){
    s = NULL; // get rid of warning
    x->x_count = 1;
    for(short i = 0; i < ac; i++)
        if(av[i].a_type == A_FLOAT)
            x->x_coef[x->x_count++] = av[i].a_w.w_float;
    update_cheby_func(x);
    x->x_arrayset = 0;
}

static t_int *shaper_perform(t_int *w){
    t_shaper *x = (t_shaper *) (w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    double xnm1 = x->x_xnm1;
    double ynm1 = x->x_ynm1;
    int n = (int)(w[4]);
    t_word *buf = (t_word *)x->x_buffer->c_vectors[0];
    double maxidx = (double)(x->x_buffer->c_npts - 1);
    while(n--){
        double yn, xn;
        float output = 0;
        double ph = ((double)*in++ + 1) * 0.5; // get phase (0-1)
        while(ph < 0) // wrap
            ph++;
        while(ph >= 1)
            ph--;
        if(x->x_arrayset && x->x_buffer->c_playable){
            double pos = ph * maxidx;
            int ndx = (int)pos;
            double frac = pos - (double)ndx;
            int ndxm1 = (ndx == 0 ? 0 : ndx - 1), ndx1 = ndx + 1, ndx2 = ndx + 2;
            if(ndxm1 < 0)
                ndxm1 = maxidx - ndxm1;
            if(ndx1 >= maxidx)
                ndx1 -= maxidx;
            if(ndx2 >= maxidx)
                ndx2 -= maxidx;
            double a = buf[ndxm1].w_float;
            double b = buf[ndx].w_float;
            double c = buf[ndx1].w_float;
            double d = buf[ndx2].w_float;
            output = interp_spline(frac, a, b, c, d);
        }
        else{
            int i = (int)(ph * (double)(FLEN - 1));
            output = x->x_cheby[i];
        }
        xn = yn = (double)output;
        if(x->x_dc_filter){
            yn = xn - xnm1 + (x->x_a * ynm1);
            output = (float)yn;
        }
        *out++ = output;
        xnm1 = xn;
        ynm1 = yn;
    }
    x->x_xnm1 = xnm1;
    x->x_ynm1 = ynm1;
    return(w+5);
}

static void shaper_dsp(t_shaper *x, t_signal **sp){
    if(sp[0]->s_sr != x->x_sr){
        x->x_sr = sp[0]->s_sr;
        x->x_a = 1 - (5*TWO_PI/(double)x->x_sr);
    }
    buffer_checkdsp(x->x_buffer);
    dsp_add(shaper_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

static void shaper_free(t_shaper *x){
    buffer_free(x->x_buffer);
    free(x->x_cheby);
    free(x->x_coef);
}

static void *shaper_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_shaper *x = (t_shaper *)pd_new(shaper_class);
    t_symbol *name = &s_;
    x->x_cheby = (float *)calloc(FLEN, sizeof(float));
    x->x_coef = (float *)calloc(MAX_COEF, sizeof(float));
    x->x_count = 2;
    x->x_coef[0] = 0;
    x->x_coef[1] = 1;
    x->x_norm = 1;
    x->x_dc_filter = 1;
    x->x_arrayset = 0;
    x->x_a = 1 - (5*TWO_PI/(double)sys_getsr());
    int argn = 0;
    if(ac){
        x->x_count = 1;
        x->x_coef[1] = 0;
        while(ac){
            if(av->a_type == A_FLOAT){
                argn = 1;
                x->x_coef[x->x_count++] = atom_getfloatarg(0, ac, av);
                ac--, av++;
            }
            else if(av->a_type == A_SYMBOL && !argn){
                t_symbol *curarg = atom_getsymbolarg(0, ac, av);
                if(curarg == gensym("-norm")){
                    if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                        t_float curfloat = atom_getfloatarg(1, ac, av);
                        x->x_norm = (int)(curfloat != 0);
                        ac-=2, av+=2;
                    }
                    else
                        goto errstate;
                }
                else if(curarg == gensym("-dc")){
                    if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                        x->x_coef[0] = atom_getfloatarg(1, ac, av);
                        ac-=2, av+=2;
                    }
                    else
                        goto errstate;
                }
                else if(curarg == gensym("-filter")){
                    if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                        x->x_dc_filter = atom_getfloatarg(1, ac, av) != 0;
                        ac-=2, av+=2;
                    }
                    else
                        goto errstate;
                }
                else if(!x->x_arrayset){
                    argn = 1;
                    name = curarg;
                    x->x_arrayset = 1, ac--, av++;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
    };
    x->x_buffer = buffer_init((t_class *)x, name, 1, 0, 0); // just added fifth argument: see buffer.c / buffer.h 
    if(!x->x_arrayset)
        update_cheby_func(x);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
    errstate:
        post("[shaper~]: improper args");
        return(NULL);
}

void shaper_tilde_setup(void){
    shaper_class = class_new(gensym("shaper~"), (t_newmethod)shaper_new,
        (t_method)shaper_free,sizeof(t_shaper), 0, A_GIMME, 0);
    class_addmethod(shaper_class, nullfn, gensym("signal"), 0);
    class_addmethod(shaper_class, (t_method)shaper_dsp, gensym("dsp"), A_CANT,  0);
    class_addmethod(shaper_class, (t_method)shaper_list, gensym("list"), A_GIMME, 0);
    class_addmethod(shaper_class, (t_method)shaper_norm, gensym("norm"), A_DEFFLOAT, 0);
    class_addmethod(shaper_class, (t_method)shaper_dc, gensym("dc"), A_DEFFLOAT, 0);
    class_addmethod(shaper_class, (t_method)shaper_filter, gensym("filter"), A_DEFFLOAT, 0);
    class_addmethod(shaper_class, (t_method)shaper_set, gensym("set"), A_SYMBOL, 0);
}
