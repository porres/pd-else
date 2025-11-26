// Porres 2017

#include <m_pd.h>
#include <math.h>

static t_class *pulsecount_class;

typedef struct _pulsecount{
    t_object  x_obj;
    t_float   x_count;
    t_float   x_page;
    t_float   x_output1;
    t_float   x_output2;
    t_int     x_max;
    t_int     x_maxpages;
    t_float   x_lastin;
    t_inlet  *x_triglet;
    t_outlet *x_outlet;
    t_outlet *x_out_pages;
}t_pulsecount;

static t_int *pulsecount_perform(t_int *w){
    t_pulsecount *x = (t_pulsecount *)(w[1]);
    int n = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out1 = (t_float *)(w[5]);
    t_float *out2 = (t_float *)(w[6]);
    t_float lastin = x->x_lastin;
    t_float output1 = x->x_output1, output2 = x->x_output2;
    t_float count = x->x_count, page = x->x_page;
    while(n--){
        t_float in = *in1++;
        t_float trig = *in2++;
        int pulse = (in > 0 && lastin <= 0);
        if(trig > 0){
            count = -1;
            page = 0;
        }
        if(pulse){
            count++;
            if(x->x_max > 0 && count >= x->x_max){
                count = 0;
                page++;
                if(x->x_maxpages > 0 && page >= x->x_maxpages){
                    page = 0;
                }
            }
            output1 = count;
            output2 = page;
        }
        *out1++ = output1;
        *out2++ = output2;
        lastin = in;
    }
    x->x_output1 = output1;
    x->x_output2 = output2;
    x->x_lastin = lastin;
    x->x_count = count;
    x->x_page = page;
    return(w+7);
}

static void pulsecount_set(t_pulsecount *x, t_floatarg f){
    x->x_count = (int)f - 1;
    if(x->x_count < 0)
        x->x_count = -1;
}

static void pulsecount_setpage(t_pulsecount *x, t_floatarg f){
    x->x_page = (int)f - 1;
    if(x->x_page < 0)
        x->x_page = -1;
}

static void pulsecount_max(t_pulsecount *x, t_floatarg f){
    x->x_max = (int)f;
    if(x->x_max < 1)
        x->x_max = -1;
}

static void pulsecount_pages(t_pulsecount *x, t_floatarg f){
    x->x_maxpages = (int)f;
    if(x->x_maxpages < 1)
        x->x_maxpages = -1;
}

static void pulsecount_reset(t_pulsecount *x){
    x->x_count = -1;
    x->x_page = 0;
}

static void pulsecount_dsp(t_pulsecount *x, t_signal **sp){
    dsp_add(pulsecount_perform, 6, x, sp[0]->s_n,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *pulsecount_free(t_pulsecount *x){
    inlet_free(x->x_triglet);
    outlet_free(x->x_outlet);
    outlet_free(x->x_out_pages);
    return (void *)x;
}

static void *pulsecount_new(t_floatarg f1, t_floatarg f2){
    t_pulsecount *x = (t_pulsecount *)pd_new(pulsecount_class);
    x->x_lastin = 1;
    x->x_count = x->x_output1 = x->x_output2 = -1;
    x->x_page = 0;
    x->x_max = (int)f1;
    if(x->x_max < 1)
       x->x_max = -1;
    x->x_maxpages = (int)f2;
    if(x->x_maxpages < 1)
       x->x_maxpages = -1;
    x->x_triglet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_out_pages = outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void pulsecount_tilde_setup(void){
    pulsecount_class = class_new(gensym("pulsecount~"),
        (t_newmethod)pulsecount_new, (t_method)pulsecount_free,
        sizeof(t_pulsecount), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(pulsecount_class, nullfn, gensym("signal"), 0);
    class_addmethod(pulsecount_class, (t_method) pulsecount_dsp, gensym("dsp"), A_CANT, 0);
    class_addbang(pulsecount_class, pulsecount_reset);
    class_addmethod(pulsecount_class, (t_method)pulsecount_max, gensym("max"), A_DEFFLOAT, 0);
    class_addmethod(pulsecount_class, (t_method)pulsecount_pages, gensym("pages"), A_DEFFLOAT, 0);
    class_addmethod(pulsecount_class, (t_method)pulsecount_set, gensym("set"), A_DEFFLOAT, 0);
    class_addmethod(pulsecount_class, (t_method)pulsecount_setpage, gensym("setpage"), A_DEFFLOAT, 0);
}
